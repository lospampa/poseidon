
/* File that contains the variable declarations */
#include <stdio.h>
#include "aurora.h"

/* First function called. It initiallizes all the functions and variables used by AURORA */
void lib_init(int metric, int start_search){
        int i, fd;
	char set[2];
        int numCores = sysconf(_SC_NPROCESSORS_ONLN);
        /*Initialization of RAPL */
        lib_detect_cpu();
        lib_detect_packages();
        /*End initialization of RAPL */

        /* Initialization of the variables necessary to perform the search algorithm */
        for(i=0;i<MAX_KERNEL;i++){
                libKernels[i].numThreads = numCores;
                libKernels[i].startThreads = 2;
                libKernels[i].numCores = numCores;
                libKernels[i].initResult = 0.0;
                libKernels[i].state = REPEAT;
                libKernels[i].metric = metric;
		libKernels[i].bestFreq = TURBO_ON;
                libKernels[i].timeTurboOff = 0.0;
                libKernels[i].timeTurboOn = 0.0;
                libKernels[i].idSeq = -1;
                idKernels[i] = 0;
                
        }

        /* Start the counters for energy and time for all the application execution */
        id_actual_region = MAX_KERNEL-1;
        lib_start_rapl_sysfs();
        initGlobalTime = omp_get_wtime();
        
        sprintf(set, "%d", TURBO_ON);
        fd = open("/sys/devices/system/cpu/cpufreq/boost", O_WRONLY);
        write(fd, set, sizeof(set));
        close(fd);    
        write_file_threshold = 0.000136;
}


/* It defines the number of threads that will execute the actual parallel region based on the current state of the search algorithm */
int lib_resolve_num_threads(uintptr_t ptr_region){
        double time=0, energy=0, result=0;
	int i, fd;
        int var_thread = 0;
	char set[2];
        id_actual_region = -1;

        /* Find the actual parallel region */
        for(i=0;i<totalKernels;i++){
                if(idKernels[i] == ptr_region){
                        id_actual_region = i;  
                        break;
                }
        }

        /* If a new parallel region is discovered */
        if(id_actual_region == -1){
                idKernels[totalKernels] = ptr_region;
                id_actual_region = totalKernels; 
                libKernels[id_actual_region].idSeq = id_actual_region + 1;
                totalKernels++;                     
        }        
         
        /* Informs the actual parallel region which was the previous parallel region and Informs the previous parallel region which is the next parallel region*/
        libKernels[id_actual_region].idParAnt = id_previous_region;
	libKernels[id_previous_region].idParPos = id_actual_region;

        if(libKernels[id_actual_region].state != END){
                /* Check the metric that is being evaluated and collect the results */
                switch(libKernels[id_actual_region].metric){
                        case PERFORMANCE:
                                result = omp_get_wtime() - libKernels[id_actual_region].initResult;
				time = result;
                                break;
                        case EDP:
                                time = omp_get_wtime() - libKernels[id_actual_region].initResult;
                                energy = lib_end_rapl_sysfs();
                                result = time * energy;
                                /* If the result is negative, it means some problem while reading of the hardware counter. Then, the metric changes to performance */
                                if(result == 0.00000 || result < 0){
                                        libKernels[id_actual_region].state = REPEAT;
                                        libKernels[id_actual_region].metric = PERFORMANCE;
                                }
                                break;
                }      
        switch(libKernels[id_actual_region].state){
		case REPEAT:
		        libKernels[id_actual_region].state = S0;
		        libKernels[id_actual_region].numThreads = libKernels[id_actual_region].startThreads;
			var_thread=libKernels[id_actual_region].numThreads;
			libKernels[id_actual_region].lastThread = libKernels[id_actual_region].numThreads; 
                        break;
		case S0:
			libKernels[id_actual_region].bestResult = result;
			libKernels[id_actual_region].bestTime = time;
			libKernels[id_actual_region].bestThreadOn = libKernels[id_actual_region].numThreads;
			libKernels[id_actual_region].numThreads = libKernels[id_actual_region].numThreads*2;
			var_thread=libKernels[id_actual_region].numThreads;
			libKernels[id_actual_region].state = S1;
			break;
		case S1:
			if(result < libKernels[id_actual_region].bestResult){
				libKernels[id_actual_region].bestResult = result;
				libKernels[id_actual_region].bestTime = time;
				libKernels[id_actual_region].bestThreadOn = libKernels[id_actual_region].numThreads;
				if(libKernels[id_actual_region].numThreads * 2 <= libKernels[id_actual_region].numCores){
					libKernels[id_actual_region].lastThread = libKernels[id_actual_region].numThreads;
					libKernels[id_actual_region].numThreads = libKernels[id_actual_region].numThreads*2;
					var_thread=libKernels[id_actual_region].numThreads;
				}
				else{
					libKernels[id_actual_region].pass = libKernels[id_actual_region].lastThread/2;
					if(libKernels[id_actual_region].pass >= 2){
					        libKernels[id_actual_region].numThreads = libKernels[id_actual_region].numThreads - libKernels[id_actual_region].pass;
					        var_thread=libKernels[id_actual_region].numThreads;
						libKernels[id_actual_region].state = S2;
					}else{
						libKernels[id_actual_region].bestFreq = TURBO_OFF; //testar com turbo off;
                                                libKernels[id_actual_region].timeTurboOn = time;
		        			libKernels[id_actual_region].state = PASS;
					}
				}
			}else{
				if(libKernels[id_actual_region].bestThreadOn == libKernels[id_actual_region].numCores/2){
        				libKernels[id_actual_region].bestFreq = TURBO_OFF;
                                        libKernels[id_actual_region].timeTurboOn = time;
					libKernels[id_actual_region].state = PASS;
                                  
				}else{
					libKernels[id_actual_region].pass = libKernels[id_actual_region].lastThread/2;
					if(libKernels[id_actual_region].pass >= 2){
						libKernels[id_actual_region].numThreads = libKernels[id_actual_region].numThreads + libKernels[id_actual_region].pass;
						var_thread=libKernels[id_actual_region].numThreads;
						libKernels[id_actual_region].state = S2;
					}else{
						libKernels[id_actual_region].bestFreq = TURBO_OFF;
                                                libKernels[id_actual_region].timeTurboOn = time;
						libKernels[id_actual_region].state = PASS;
					}
				}
			}
			break;
		case S2:
			if(libKernels[id_actual_region].bestResult < result){
				libKernels[id_actual_region].pass = libKernels[id_actual_region].pass/2;
				if(libKernels[id_actual_region].pass >= 2){
					libKernels[id_actual_region].numThreads = libKernels[id_actual_region].numThreads + libKernels[id_actual_region].pass;
					var_thread=libKernels[id_actual_region].numThreads;
				}
				else{
					libKernels[id_actual_region].bestFreq = TURBO_OFF;
                                        libKernels[id_actual_region].timeTurboOn = time;
					libKernels[id_actual_region].state = PASS;
				}
			}else{
				libKernels[id_actual_region].bestThreadOn = libKernels[id_actual_region].numThreads;
				libKernels[id_actual_region].bestTime = time;
				libKernels[id_actual_region].bestResult = result;
				libKernels[id_actual_region].pass = libKernels[id_actual_region].pass/2;
				if(libKernels[id_actual_region].pass >= 2){
					libKernels[id_actual_region].numThreads += libKernels[id_actual_region].pass;
					var_thread=libKernels[id_actual_region].numThreads;
				}else{
					libKernels[id_actual_region].bestFreq = TURBO_OFF;
                                        libKernels[id_actual_region].timeTurboOn = time;
					libKernels[id_actual_region].state = PASS;
				}
			}
		        break;
                case END_THREADS:
				libKernels[id_actual_region].state = END;
				libKernels[id_actual_region].timeTurboOff = time;
				if(libKernels[id_actual_region].bestResult < result){
					libKernels[id_actual_region].bestFreq = TURBO_ON;
                        	}
                        	var_thread = libKernels[id_actual_region].bestThreadOn;
                        	break;
        	}
		
		if(libKernels[id_actual_region].state != END_THREADS && libKernels[id_actual_region].bestFreq != libKernels[id_previous_region].bestFreq && libKernels[id_actual_region].bestResult > write_file_threshold){
                	fd = open("/sys/devices/system/cpu/cpufreq/boost", O_WRONLY);
                        sprintf(set, "%d", libKernels[id_actual_region].bestFreq);
			write(fd, set, sizeof(set));
			close(fd);
                }
		id_previous_region = id_actual_region;
		return var_thread;
	}else{
		if((libKernels[id_previous_region].bestFreq == TURBO_OFF && libKernels[id_actual_region].bestFreq == TURBO_ON && (libKernels[id_actual_region].timeTurboOn + write_file_threshold < libKernels[id_actual_region].timeTurboOff)) || (libKernels[id_previous_region].bestFreq == TURBO_ON && libKernels[id_actual_region].bestFreq == TURBO_OFF && (libKernels[id_actual_region].timeTurboOff + write_file_threshold < libKernels[id_actual_region].timeTurboOn))){
			fd = open("/sys/devices/system/cpu/cpufreq/boost", O_WRONLY);
			sprintf(set, "%d", libKernels[id_actual_region].bestFreq);
			write(fd, set, sizeof(set));
			close(fd);
                }
		id_previous_region = id_actual_region;
		return libKernels[id_actual_region].bestThreadOn;
	}

        
}

/* It finalizes the environment of Aurora */
void lib_destructor(){
        float time = omp_get_wtime() - initGlobalTime;
        id_actual_region = MAX_KERNEL-1;
        float energy = lib_end_rapl_sysfs();
        float edp = time * energy;
        printf("Poseidon - Execution Time: %.5f seconds\n", time);
        printf("Poseidon - Energy: %.5f joules\n",energy);
        printf("Poseidon - EDP: %.5f\n", edp);
}

/* Function used by the Intel RAPL to detect the CPU Architecture*/
void lib_detect_cpu(){                               
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
void lib_detect_packages(){
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
void lib_start_rapl_sysfs(){
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
                                        fscanf(fff,"%lld",&libKernels[id_actual_region].kernelBefore[j][i]);
                                        fclose(fff);
                                }
                        }
                }
        }
}

/* Function used by the Intel RAPL to load the value of the hardware counter and returns the energy consumption*/
double lib_end_rapl_sysfs(){
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
                                fscanf(fff,"%lld",&libKernels[id_actual_region].kernelAfter[j][i]);
                                fclose(fff);
                        }
                }
                }
        }
        for(j=0;j<total_packages;j++) {
                for(i=0;i<NUM_RAPL_DOMAINS;i++) {
                        if(valid[j][i]){
                                if(strcmp(event_names[j][i],"core")!=0 && strcmp(event_names[j][i],"uncore")!=0){
                                        total += (((double)libKernels[id_actual_region].kernelAfter[j][i]-(double)libKernels[id_actual_region].kernelBefore[j][i])/1000000.0);
                                }
                        }
                }
        }
        return total;
}
