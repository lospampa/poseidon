/* File that contains the variable declarations */
#include "aurora.h" 


/* First function called. It initiallizes all the functions and variables used by AURORA */
void aurora_init(int aurora, int start_search){
        int i, startThreads=0;
        int numCores = sysconf(_SC_NPROCESSORS_ONLN);
        /*Initialization of RAPL */
        aurora_detect_cpu(); 
        aurora_detect_packages();
        /*End initialization of RAPL */

        /* Initialization of the variables necessary to perform the search algorithm */
		if(start_search == 0){
		        startThreads = numCores;
		        while(startThreads != 2 && startThreads != 3 && startThreads != 5){
		                startThreads = startThreads/2;
		        }
		}else{
			startThreads = start_search;
		}
        for(i=0;i<MAX_KERNEL;i++){
                auroraKernels[i].startThreads = startThreads;
                auroraKernels[i].numThreads = numCores;
                auroraKernels[i].numCores = numCores;
                auroraKernels[i].initResult = 0.0;
		auroraKernels[i].total_region_perf = 0.0;
		auroraKernels[i].total_region_energy = 0.0;
		auroraKernels[i].total_region_edp = 0.0;
                auroraKernels[i].state = REPEAT;
                auroraKernels[i].auroraMetric = aurora;
                auroraKernels[i].lastResult = 0.0;
		auroraKernels[i].bestTurbo = TURBO_OFF;
		auroraKernels[i].bestGame = GAME_OFF;
                idKernels[i]=0;
        }

        /* Start the counters for energy and time for all the application execution */
        id_actual_region = MAX_KERNEL-1;
        aurora_start_rapl_sysfs();
        initGlobalTime = omp_get_wtime();
	FILE *turbo = fopen("/sys/devices/system/cpu/cpufreq/boost", "wt");
        int var = 0;
        fprintf(turbo, "%d", var);
        fclose(turbo);
	
}

/* It defines the number of threads that will execute the actual parallel region based on the current state of the search algorithm */
int aurora_resolve_num_threads(uintptr_t ptr_region){
        int i;
        id_actual_region = -1;
        /* Find the actual parallel region */
        for(i=0;i<totalKernels;i++){
                if(idKernels[i] == ptr_region){
                        id_actual_region=i;
                        break;
                }
        }
        /* If a new parallel region is discovered */
        if(id_actual_region == -1){
                idKernels[totalKernels] = ptr_region;
                id_actual_region = totalKernels;
                totalKernels++;
        }
        /* Check the state of the search algorithm. */
        switch(auroraKernels[id_actual_region].state){
		case END_THREADS:
                        turbo = fopen("/sys/devices/system/cpu/cpufreq/boost", "wt");
                        auroraKernels[id_actual_region].initResult = omp_get_wtime();
                        var = 1;
                        fprintf(turbo, "%d", var);
                        fclose(turbo);
                        return auroraKernels[id_actual_region].bestThread;

                case END_TURBO:
                        auroraKernels[id_actual_region].initResult = omp_get_wtime();
                        //MATHEUS: escrever nos cores a frequencia de operação do game mode.
                        return auroraKernels[id_actual_region].bestThread;
		case END:
			turbo = fopen("/sys/devices/system/cpu/cpufreq/boost", "wt");
                        auroraKernels[id_actual_region].initResult = omp_get_wtime();  /* It is useful only if the continuous adaptation is enable. Otherwise, it can be disabled */
                        if(auroraKernels[id_actual_region].bestTurbo == TURBO_ON && auroraKernels[id_actual_region].bestGame == GAME_OFF){
                                var = 1;
                                fprintf(turbo, "%d", var);
                        }else if(auroraKernels[id_actual_region].bestGame == GAME_ON){
                                var = 0;
                                fprintf(turbo, "%d", var); //deixa turbo core off.
                                printf("DEBUG: Ativou Game Mode\n");
                                //MATHEUS: Setar frequencia de game mode nos cores.
                        }else{
                                var = 0;
                                fprintf(turbo, "%d", var);
                        }
                        fclose(turbo);

                        return auroraKernels[id_actual_region].bestThread;
                default:
                        aurora_start_rapl_sysfs();
                        auroraKernels[id_actual_region].initResult = omp_get_wtime();
                        return auroraKernels[id_actual_region].numThreads;
        }

}

/* It is responsible for performing the search algorithm */
void aurora_end_parallel_region(){
        double time, energy, result=0, ratio;
	int var;
        FILE *turbo = fopen("/sys/devices/system/cpu/cpufreq/boost", "wt");
        if(auroraKernels[id_actual_region].state !=END){
                /* Check the metric that is being evaluated and collect the results */
                switch(auroraKernels[id_actual_region].auroraMetric){
                        case PERFORMANCE:
                                time = omp_get_wtime();
                                result = time - auroraKernels[id_actual_region].initResult;
                                break;
                        case ENERGY:
                                result = aurora_end_rapl_sysfs();
                                /* If the result is negative, it means some problem while reading of the hardware counter. Then, the metric changes to performance */
                                if(result == 0.000000 || result < 0){
                                        auroraKernels[id_actual_region].state = REPEAT;
                                        auroraKernels[id_actual_region].auroraMetric = PERFORMANCE;
                                }
                                break;
                        case EDP:
                                time = omp_get_wtime() - auroraKernels[id_actual_region].initResult;
                                energy = aurora_end_rapl_sysfs();
                                result = time * energy;
                                /* If the result is negative, it means some problem while reading of the hardware counter. Then, the metric changes to performance */
                                if(result == 0.00000 || result < 0){
                                        auroraKernels[id_actual_region].state = REPEAT;
                                        auroraKernels[id_actual_region].auroraMetric = PERFORMANCE;
                                }
                                break;
			 case POWER:
                                        time = omp_get_wtime() - auroraKernels[id_actual_region].initResult;
                                        energy = aurora_end_amd_msr();
                                        result = energy/time;
                                        if(result == 0.00000 || result < 0){
                                                auroraKernels[id_actual_region].state = REPEAT;
                                                auroraKernels[id_actual_region].auroraMetric = PERFORMANCE;
                                        }
                                break;
                        case TEMPERATURE:
                                        time = omp_get_wtime() - auroraKernels[id_actual_region].initResult;
                                        energy = aurora_end_amd_msr();
                                        result = sqrt( (energy*energy)+(time*time));
                                        auroraKernels[id_actual_region].total_region_perf += time;
                                        auroraKernels[id_actual_region].steps++;
                                        if(time < auroraKernels[id_actual_region].bestTime)
                                                auroraKernels[id_actual_region].bestTime = time;
                                        if(result == 0.00000 || result < 0){
                                                auroraKernels[id_actual_region].state = REPEAT;
                                                auroraKernels[id_actual_region].auroraMetric = PERFORMANCE;
                                        }
                                break;

                }
                switch(auroraKernels[id_actual_region].state){
                        case REPEAT:
                                auroraKernels[id_actual_region].state = S0;
                                auroraKernels[id_actual_region].numThreads = auroraKernels[id_actual_region].startThreads; 
                                auroraKernels[id_actual_region].lastThread = auroraKernels[id_actual_region].numThreads;
                                break;
                        case S0:
                                auroraKernels[id_actual_region].bestResult = result;
                                auroraKernels[id_actual_region].bestThread = auroraKernels[id_actual_region].numThreads;
                                auroraKernels[id_actual_region].numThreads = auroraKernels[id_actual_region].bestThread*2; 
                                auroraKernels[id_actual_region].state = S1;
                                break;
                        case S1:
                                if(result < auroraKernels[id_actual_region].bestResult){ //comparing S0 to REPEAT
                                        auroraKernels[id_actual_region].bestResult = result;
                                        auroraKernels[id_actual_region].bestThread = auroraKernels[id_actual_region].numThreads;
                                        /* if there are opportunities for improvements, then double the number of threads */
                                        if(auroraKernels[id_actual_region].numThreads * 2 <= auroraKernels[id_actual_region].numCores){
                                                auroraKernels[id_actual_region].lastThread = auroraKernels[id_actual_region].numThreads;
                                                auroraKernels[id_actual_region].numThreads = auroraKernels[id_actual_region].bestThread*2;
                                                auroraKernels[id_actual_region].state = S1;
                                        }else{
                                                /* It means that the best number so far is equal to the number of cores */
                                                /* Then, it will realize a guided search near to this number */
                                                auroraKernels[id_actual_region].pass = auroraKernels[id_actual_region].lastThread/2;                                                
                                                auroraKernels[id_actual_region].numThreads = auroraKernels[id_actual_region].numThreads  - auroraKernels[id_actual_region].pass;
                                                if(auroraKernels[id_actual_region].pass == 1)
                                                        auroraKernels[id_actual_region].state = S3;        
						else
	                                                auroraKernels[id_actual_region].state = S2;
                                        }
                                }else{ 
                                        /* Thread scalability stopped */
                                        /* Find the interval of threads that provided this result. */
                                        /* if the best number of threads so far is equal to the number of cores, then go to.. */
                                        if(auroraKernels[id_actual_region].bestThread == auroraKernels[id_actual_region].numCores/2){
                                                auroraKernels[id_actual_region].pass = auroraKernels[id_actual_region].lastThread/2;                                                
                                                auroraKernels[id_actual_region].numThreads = auroraKernels[id_actual_region].numThreads  - auroraKernels[id_actual_region].pass;
                                                if(auroraKernels[id_actual_region].pass == 1)
                                                        auroraKernels[id_actual_region].state = S3;        
        					else
		                                        auroraKernels[id_actual_region].state = S2;
                                        }else{
                                                auroraKernels[id_actual_region].pass = auroraKernels[id_actual_region].lastThread/2;                                                
                                                auroraKernels[id_actual_region].numThreads = auroraKernels[id_actual_region].numThreads  + auroraKernels[id_actual_region].pass;
                                                if(auroraKernels[id_actual_region].pass == 1)
                                                        auroraKernels[id_actual_region].state = S3;        
                                                else
														auroraKernels[id_actual_region].state = S2;  
                                        }

                                }
                                break;
                        case S2:        
                                if(auroraKernels[id_actual_region].bestResult < result){
                                        auroraKernels[id_actual_region].pass = auroraKernels[id_actual_region].pass/2;
                                        auroraKernels[id_actual_region].numThreads = auroraKernels[id_actual_region].numThreads + auroraKernels[id_actual_region].pass;
                                        if(auroraKernels[id_actual_region].pass == 1)
                                                auroraKernels[id_actual_region].state = S3;
                                        else
                                                auroraKernels[id_actual_region].state = S2;
                                }else{
                                        auroraKernels[id_actual_region].bestThread = auroraKernels[id_actual_region].numThreads;
                                        auroraKernels[id_actual_region].bestResult = result;
                                        auroraKernels[id_actual_region].pass = auroraKernels[id_actual_region].pass/2;
                                        auroraKernels[id_actual_region].numThreads = auroraKernels[id_actual_region].numThreads + auroraKernels[id_actual_region].pass;
                                        if(auroraKernels[id_actual_region].pass == 1)
                                                auroraKernels[id_actual_region].state = S3;
                                        else
                                                auroraKernels[id_actual_region].state = S2;
                                }
                                break;                        
                        case S3: //The last comparison to define the best number of threads
                                 if(result < auroraKernels[id_actual_region].bestResult){
                                        auroraKernels[id_actual_region].bestThread = auroraKernels[id_actual_region].numThreads;
                                        auroraKernels[id_actual_region].state = END_THREADS;
                                        var = 1;
                                        fprintf(turbo, "%d", var);
                                }else{
                                        auroraKernels[id_actual_region].state = END_THREADS;
                                        var = 1;
                                        fprintf(turbo, "%d", var);
                                }
                                break;
                        case END_THREADS:
                                if(result < auroraKernels[id_actual_region].bestResult){
                                        auroraKernels[id_actual_region].bestTurbo = TURBO_ON;
                                        auroraKernels[id_actual_region].state = END_TURBO;
                                        //MATHEUS: executar com game mode (setar cores para a frequencia do game mode)
                                }else{
                                        auroraKernels[id_actual_region].bestTurbo = TURBO_OFF;
                                        auroraKernels[id_actual_region].state = END_TURBO;
                                        //MATHEUS: executar com game mode (setar cores para a frequencia do game mode)
                                }
                                break;
                        case END_TURBO:
                                if(result < auroraKernels[id_actual_region].bestResult){
                                        auroraKernels[id_actual_region].bestGame = GAME_ON;
                                        auroraKernels[id_actual_region].state = END;
                                }else{
                                        auroraKernels[id_actual_region].bestTurbo = GAME_OFF;
                                        auroraKernels[id_actual_region].state = END;
                                }
                                break;
                        }

                }
        }else{
		var = 1;
              	fprintf(turbo, "%d", var);
                result = omp_get_wtime() - auroraKernels[id_actual_region].initResult;
                if(result > 0.1){
                        if(auroraKernels[id_actual_region].lastResult == 0.0){
                                auroraKernels[id_actual_region].lastResult = result;
                        }else{
                                ratio = auroraKernels[id_actual_region].lastResult/result;
                                if(ratio < 0.7 || ratio > 1.3){
                                        auroraKernels[id_actual_region].numThreads = auroraKernels[id_actual_region].numCores;
                                        auroraKernels[id_actual_region].lastResult = 0.0;
                                        auroraKernels[id_actual_region].state = REPEAT;
                                }else{
                                        auroraKernels[id_actual_region].lastResult = result;
                                }
                        }
                }
        }
	fclose(turbo);
}

/* It finalizes the environment of Aurora */
void aurora_destructor(){
        float time = omp_get_wtime() - initGlobalTime;
        id_actual_region = MAX_KERNEL-1;
        float energy = aurora_end_rapl_sysfs();
        float edp = time * energy;
        printf("AURORA - Execution Time: %.5f seconds\n", time);
        printf("AURORA - Energy: %.5f joules\n",energy);
        printf("AURORA - EDP: %.5f\n",edp);
}

/* Function used by the Intel RAPL to detect the CPU Architecture*/
void aurora_detect_cpu(){
        FILE *fff;
        int family,model=-1;
        char buffer[BUFSIZ],*result;
        char vendor[BUFSIZ];
        fff=fopen("/proc/cpuinfo","r");
        while(1) {
                result=fgets(buffer,BUFSIZ,fff);
                if (result==NULL)
                        break;
                if (!strncmp(result,"vendor_id",8)) {
                        sscanf(result,"%*s%*s%s",vendor);
                        if (strncmp(vendor,"GenuineIntel",12)) {
                                printf("%s not an Intel chip\n",vendor);
                        }
                }
                if (!strncmp(result,"cpu family",10)) {
                        sscanf(result,"%*s%*s%*s%d",&family);
                        if (family!=6) {
                                printf("Wrong CPU family %d\n",family);
                        }
                }
                if (!strncmp(result,"model",5)) {
                        sscanf(result,"%*s%*s%d",&model);
                }
        }
        fclose(fff);
}

/* Function used by the Intel RAPL to detect the number of cores and CPU sockets*/
void aurora_detect_packages(){
        char filename[BUFSIZ];
        FILE *fff;
        int package;
        int i;
        for(i=0;i<MAX_PACKAGES;i++)
                package_map[i]=-1;
        for(i=0;i<MAX_CPUS;i++) {
                sprintf(filename,"/sys/devices/system/cpu/cpu%d/topology/physical_package_id",i);
                fff=fopen(filename,"r");
                if (fff==NULL)
                        break;
                fscanf(fff,"%d",&package);
                fclose(fff);
                if (package_map[package]==-1) {
                        total_packages++;
                        package_map[package]=i;
                }
        }
        total_cores=i;
}

/* Function used by the Intel RAPL to store the actual value of the hardware counter*/
void aurora_start_rapl_sysfs(){
        int i,j;
        FILE *fff;
        for(j=0;j<total_packages;j++) {
                i=0;
                sprintf(packname[j],"/sys/class/powercap/intel-rapl/intel-rapl:%d",j);
                sprintf(tempfile,"%s/name",packname[j]);
                fff=fopen(tempfile,"r");
                if (fff==NULL) {
                        fprintf(stderr,"\tCould not open %s\n",tempfile);
                        exit(0);
                }
                fscanf(fff,"%s",event_names[j][i]);
                valid[j][i]=1;
                fclose(fff);
                sprintf(filenames[j][i],"%s/energy_uj",packname[j]);

                /* Handle subdomains */
                for(i=1;i<NUM_RAPL_DOMAINS;i++){
                        sprintf(tempfile,"%s/intel-rapl:%d:%d/name", packname[j],j,i-1);
                        fff=fopen(tempfile,"r");
                        if (fff==NULL) {
                                //fprintf(stderr,"\tCould not open %s\n",tempfile);
                                valid[j][i]=0;
                                continue;
                        }
                        valid[j][i]=1;
                        fscanf(fff,"%s",event_names[j][i]);
                        fclose(fff);
                        sprintf(filenames[j][i],"%s/intel-rapl:%d:%d/energy_uj", packname[j],j,i-1);
                }
        }
 /* Gather before values */
        for(j=0;j<total_packages;j++) {
                for(i=0;i<NUM_RAPL_DOMAINS;i++) {
                        if(valid[j][i]) {
                                fff=fopen(filenames[j][i],"r");
                                if (fff==NULL) {
                                        fprintf(stderr,"\tError opening %s!\n",filenames[j][i]);
                                }
                                else {
                                        fscanf(fff,"%lld",&auroraKernels[id_actual_region].kernelBefore[j][i]);
                                        fclose(fff);
                                }
                        }
                }
        }
}

/* Function used by the Intel RAPL to load the value of the hardware counter and returns the energy consumption*/
double aurora_end_rapl_sysfs(){
        int i, j;
        FILE *fff;
        double total=0;
        for(j=0;j<total_packages;j++) {
                for(i=0;i<NUM_RAPL_DOMAINS;i++) {
                        if (valid[j][i]) {
                                fff=fopen(filenames[j][i],"r");
                        if (fff==NULL) {
                                fprintf(stderr,"\tError opening %s!\n",filenames[j][i]);
                        }
                        else {
                                fscanf(fff,"%lld",&auroraKernels[id_actual_region].kernelAfter[j][i]);
                                fclose(fff);
                        }
                }
                }
        }
        for(j=0;j<total_packages;j++) {
                for(i=0;i<NUM_RAPL_DOMAINS;i++) {
                        if(valid[j][i]){
                                if(strcmp(event_names[j][i],"core")!=0 && strcmp(event_names[j][i],"uncore")!=0){
                                        total += (((double)auroraKernels[id_actual_region].kernelAfter[j][i]-(double)auroraKernels[id_actual_region].kernelBefore[j][i])/1000000.0);
                                }
                        }
                }
        }
        return total;
}


