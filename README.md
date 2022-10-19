# parallel-fractal-image-generation---Mandelbrot-and-Julia
Generare paralela de fractali folosind multimile Mandelbrot si Julia - Parallel and distributed algorithms



In implementarea paralela a programului am realizat urmatorii pasi (fata de cea secventiala):

Am schimbat functia "get_args()" in care se citesc parametrii cu care se ruleaza programul pentru a putea accepta si numarul de thread-uri (P), acum fiind cu un parametru mai mult

Am creat o structura pentru argumentele functiei de thread cu campurile:
			-int thread_id;
			-params *par;
			-int width1;
			-int width2;
			-int height1;
			-int height2;
			
Variabila result am pastrat-o globala pentru a nu se modifica de fiecare data cand un thread se foloseste de ea

In interiorul functiilor run_julia si run_mandelbrot am creat si am instantiat doua variabile - start si end - pentru a putea face impartirea thread-urilor in executarea sarcinilor de cautare in planul complex atat pentru sarcina de aflare a multimilor, cat si pentru cea de translatie a coordonatelor

Am creat functia f() = functia de thread care va fi luata ca argument de catre functia care creaza thread-urile. In aceasta vom avea urmatoarea ordine de executare a functiilor:
			-run_julia
			-write_output_file
			-run_mandelbrot
			-write_output_file
			
Intre fiecare din ele vom folosi bariere pentru a se sincroniza thread-urile

In functia principala a programului vom face citirea de inputuri necesare, alocarea memoriei, initializarea si crearea de thread-uri si apoi eliberarea memoriei si distrugerea barierei.
