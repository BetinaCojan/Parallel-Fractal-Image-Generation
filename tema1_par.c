#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <pthread.h>

char *in_filename_julia;
char *in_filename_mandelbrot;
char *out_filename_julia;
char *out_filename_mandelbrot;
int   no_threads;

// structura pentru un numar complex
typedef struct _complex {
	double a;
	double b;
} complex;

// structura pentru parametrii unei rulari
typedef struct _params {
	int is_julia, iterations;
	double x_min, x_max, y_min, y_max, resolution;
	complex c_julia;
} params;

params algorithm_params;

typedef struct _result_params {
	int width;
	int height;
	int **result;
	pthread_barrier_t barrier;
} result_params;

result_params result_param;

// citeste argumentele programului
void get_args(int argc, char **argv)
{
	if (argc < 6) {
		printf("Numar insuficient de parametri:\n\t"
				"./tema1 fisier_intrare_julia fisier_iesire_julia "
				"fisier_intrare_mandelbrot fisier_iesire_mandelbrot P\n");
		exit(1);
	}

	in_filename_julia = argv[1];
	out_filename_julia = argv[2];
	in_filename_mandelbrot = argv[3];
	out_filename_mandelbrot = argv[4];
	no_threads = atol(argv[5]);
}

// citeste fisierul de intrare
void read_input_file(char *in_filename, params* par)
{
	FILE *file = fopen(in_filename, "r");
	if (file == NULL) {
		printf("Eroare la deschiderea fisierului de intrare!\n");
		exit(1);
	}

	fscanf(file, "%d", &par->is_julia);
	fscanf(file, "%lf %lf %lf %lf",
			&par->x_min, &par->x_max, &par->y_min, &par->y_max);
	fscanf(file, "%lf", &par->resolution);
	fscanf(file, "%d", &par->iterations);

	if (par->is_julia) {
		fscanf(file, "%lf %lf", &par->c_julia.a, &par->c_julia.b);
	}

	fclose(file);
}

// scrie rezultatul in fisierul de iesire
void write_output_file(char *out_filename, int **result, int width, int height)
{
	int i, j;

	FILE *file = fopen(out_filename, "w");
	if (file == NULL) {
		printf("Eroare la deschiderea fisierului de iesire!\n");
		return;
	}

	fprintf(file, "P2\n%d %d\n255\n", width, height);
	for (i = 0; i < height; i++) {
		for (j = 0; j < width; j++) {
			fprintf(file, "%d ", result[i][j]);
		}
		fprintf(file, "\n");
	}

	fclose(file);
}

// aloca memorie pentru rezultat
int **allocate_memory(int width, int height)
{
	int **result;
	int i;

	result = malloc(height * sizeof(int*));
	if (result == NULL) {
		printf("Eroare la malloc!\n");
		exit(1);
	}

	for (i = 0; i < height; i++) {
		result[i] = malloc(width * sizeof(int));
		if (result[i] == NULL) {
			printf("Eroare la malloc!\n");
			exit(1);
		}
	}

	return result;
}

// elibereaza memoria alocata
void free_memory(int **result, int height)
{
	int i;

	for (i = 0; i < height; i++) {
		free(result[i]);
	}
	free(result);
}

// ruleaza algoritmul Julia
void run_julia(params *par, int **result, int width, int height, int thread_number)
{
	int w, h;

	for (w = thread_number; w < width; w+=no_threads) {
		for (h = 0; h < height; h++) {
			int step = 0;
			complex z = { .a = w * par->resolution + par->x_min,
							.b = h * par->resolution + par->y_min };

			while (sqrt(pow(z.a, 2.0) + pow(z.b, 2.0)) < 2.0 && step < par->iterations) {
				complex z_aux = { .a = z.a, .b = z.b };

				z.a = pow(z_aux.a, 2) - pow(z_aux.b, 2) + par->c_julia.a;
				z.b = 2 * z_aux.a * z_aux.b + par->c_julia.b;

				step++;
			}

			// transforma rezultatul din coordonate matematice in coordonate ecran
			result[height - h - 1][w] = step % 256;
		}
	}
}

// ruleaza algoritmul Mandelbrot
void run_mandelbrot(params *par, int **result, int width, int height, int thread_number)
{
	int w, h;

	for (w = thread_number; w < width; w+=no_threads) {
		for (h = 0; h < height; h++) {
			complex c = { .a = w * par->resolution + par->x_min,
							.b = h * par->resolution + par->y_min };
			complex z = { .a = 0, .b = 0 };
			int step = 0;

			while (sqrt(pow(z.a, 2.0) + pow(z.b, 2.0)) < 2.0 && step < par->iterations) {
				complex z_aux = { .a = z.a, .b = z.b };

				z.a = pow(z_aux.a, 2.0) - pow(z_aux.b, 2.0) + c.a;
				z.b = 2.0 * z_aux.a * z_aux.b + c.b;

				step++;
			}

			// transforma rezultatul din coordonate matematice in coordonate ecran
			result[height - h - 1][w] = step % 256;
		}
	}
}

typedef struct _thread_params {
	int current_thread_number;
} thread_params;

void* run_thread(void *thread_param) {
	// Synchronisation point [0] - main threads has julia set params ready
	thread_params current_params = *( (thread_params*) thread_param);

	run_julia(&algorithm_params,
			result_param.result,
			result_param.width,
			result_param.height,
			current_params.current_thread_number);

	// Synchronisation point [1] - signal the main thread that workers computed julia set
	pthread_barrier_wait(&result_param.barrier);

	// Synchronisation point [2] - wait for the main thread to finish loading mandelbrot params
	pthread_barrier_wait(&result_param.barrier);

	run_mandelbrot(&algorithm_params,
			result_param.result,
			result_param.width,
			result_param.height,
			current_params.current_thread_number);

	// Synchronisation point [3] - finished computing mandelbrot
	return NULL;
}

int main(int argc, char *argv[])
{
	int width, height;
	int **result;

	// se citesc argumentele programului
	get_args(argc, argv);

	// Julia:
	// - se citesc parametrii de intrare
	// - se aloca tabloul cu rezultatul
	// - se ruleaza algoritmul
	// - se scrie rezultatul in fisierul de iesire
	// - se elibereaza memoria alocata
	read_input_file(in_filename_julia, &algorithm_params);

	width = (algorithm_params.x_max - algorithm_params.x_min) / algorithm_params.resolution;
	height = (algorithm_params.y_max - algorithm_params.y_min) / algorithm_params.resolution;

	result = allocate_memory(width, height);

	result_param.width = width;
	result_param.height = height;
	result_param.result = result;

	pthread_t* ids = malloc(sizeof(pthread_t) * no_threads);
	thread_params* thread_params = malloc(sizeof(struct _thread_params) * no_threads);
	pthread_barrier_init(&result_param.barrier, NULL, no_threads + 1);

	// Synchronisation point [0] - main threads has julia set params ready, start workers
	for (int i=0; i < no_threads; i++) {
		thread_params[i].current_thread_number = i;
		pthread_create(&ids[i], NULL, run_thread, &thread_params[i]);
	}

	// Synchronisation point [1] - wait for all threads to finish julia set
	pthread_barrier_wait(&result_param.barrier);

	write_output_file(out_filename_julia, result, width, height);
	free_memory(result, height);

	// Mandelbrot:
	// - se citesc parametrii de intrare
	// - se aloca tabloul cu rezultatul
	// - se ruleaza algoritmul
	// - se scrie rezultatul in fisierul de iesire
	// - se elibereaza memoria alocata
	read_input_file(in_filename_mandelbrot, &algorithm_params);

	width = (algorithm_params.x_max - algorithm_params.x_min) / algorithm_params.resolution;
	height = (algorithm_params.y_max - algorithm_params.y_min) / algorithm_params.resolution;

	result = allocate_memory(width, height);

	result_param.width = width;
	result_param.height = height;
	result_param.result = result;

	// Synchronisation point [2] - Signal threads to start computing the mandelbrot set
	pthread_barrier_wait(&result_param.barrier);

	// Synchronisation point [3] - Finished computing mandelbrot
	for (int i=0; i < no_threads; i++) {
		pthread_join(ids[i], NULL);
	}

	write_output_file(out_filename_mandelbrot, result, width, height);

	free_memory(result, height);

	return 0;
}
