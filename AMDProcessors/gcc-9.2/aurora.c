/* File that contains the variable declarations */
#include "aurora.h" 
#include <stdio.h>

/* First function called. It initiallizes all the functions and variables used by AURORA */
void aurora_init(int aurora, int start_search){
        int turbo;
	int i, startThreads=0;
	char set[2];
        int numCores = sysconf(_SC_NPROCESSORS_ONLN);
        /*Initialization of RAPL */
        //aurora_detect_cpu(); 
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
                auroraKernels[i].state = REPEAT;
                auroraKernels[i].steps = 0;
                auroraKernels[i].bestTime = 1000.00;
		auroraKernels[i].total_region_perf = 0.0;
                auroraKernels[i].auroraMetric = aurora;
                auroraKernels[i].lastResult = 0.0;
		auroraKernels[i].bestTurbo = TURBO_OFF;
		auroraKernels[i].bestGame = GAME_OFF;
                idKernels[i]=0;
		
        }

        /* Start the counters for energy and time for all the application execution */
        id_actual_region = MAX_KERNEL-1;
        aurora_start_amd_msr();
        initGlobalTime = omp_get_wtime();
	
	/*Define the turbo core as inactive*/
	sprintf(set, "%d", 1);
	turbo = open("/sys/devices/system/cpu/cpufreq/boost", O_WRONLY);
	write(turbo, set, sizeof(set));

}

/* It defines the number of threads that will execute the actual parallel region based on the current state of the search algorithm */
int aurora_resolve_num_threads(uintptr_t ptr_region){
        int i, var, turbo; 
	char set[2];
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
			turbo = open("/sys/devices/system/cpu/cpufreq/boost", O_WRONLY);
			sprintf(set, "%d", 1);
			write(turbo, set, sizeof(set));
			close(turbo);
                        auroraKernels[id_actual_region].initResult = omp_get_wtime();
		        return auroraKernels[id_actual_region].bestThread;
                case END:
                     	switch(auroraKernels[id_actual_region].bestFreq){
				case TURBO_OFF:
					turbo = open("/sys/devices/system/cpu/cpufreq/boost", O_WRONLY);
					sprintf(set, "%d", 0);
					write(turbo, set, sizeof(set));
					close(turbo);
					auroraKernels[id_actual_region].initResult = omp_get_wtime(); /* It is useful only if the continuous adaptation is enable. Otherwise, it can be disabled */
					return auroraKernels[id_actual_region].bestThread;
				case TURBO_ON:
					turbo = open("/sys/devices/system/cpu/cpufreq/boost", O_WRONLY);
					sprintf(set, "%d", 1);
					write(turbo, set, sizeof(set));
					close(turbo);
					auroraKernels[id_actual_region].initResult = omp_get_wtime(); /* It is useful only if the continuous adaptation is enable. Otherwise, it can be disabled */
					return auroraKernels[id_actual_region].bestThread;
				case GAME_ON:

				// Matheus: setar a frequencia para todos os cores.	
				
                                case GAME_OFF:
				// Matheus: A ser implementado.
			}
                case END_TURBO:
                        // Abrir arquivo e ligar GAME MODE (COMO?)
                        //auroraKernels[id_actual_region].initResult = omp_get_wtime();
		        //return auroraKernels[id_actual_region].bestThread;
				
                default:
                        aurora_start_amd_msr();
                        auroraKernels[id_actual_region].initResult = omp_get_wtime();
                        return auroraKernels[id_actual_region].numThreads;
        }

}

/* It is responsible for performing the search algorithm */
void aurora_end_parallel_region(){
        double time, energy, result=0, ratio;
	int turbo
	char set[2];
        if(auroraKernels[id_actual_region].state !=END){
                /* Check the metric that is being evaluated and collect the results */
                switch(auroraKernels[id_actual_region].auroraMetric){
                        case PERFORMANCE:
                                time = omp_get_wtime();
                                result = time - auroraKernels[id_actual_region].initResult;
                                break;
                        case ENERGY:
                                result = aurora_end_amd_msr();
                                /* If the result is negative, it means some problem while reading of the hardware counter. Then, the metric changes to performance */
                                if(result == 0.000000 || result < 0){
                                        auroraKernels[id_actual_region].state = REPEAT;
                                        auroraKernels[id_actual_region].auroraMetric = PERFORMANCE;
                                }
                                break;
                        case EDP:
                                time = omp_get_wtime() - auroraKernels[id_actual_region].initResult;
                                energy = aurora_end_amd_msr();
                                result = time * energy;
                                /* If the result is negative, it means some problem while reading of the hardware counter. Then, the metric changes to performance */
                                if(result == 0.00000 || result < 0){
                                        auroraKernels[id_actual_region].state = REPEAT;
                                        auroraKernels[id_actual_region].auroraMetric = PERFORMANCE;
                                }
                                break;
			/*
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
				*/
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
						auroraKernels[id_actual_region].state = END_THREADS;
						/* Activate Turbo Core */
						auroraKernels[id_actual_region].bestFreq = TURBO_ON;

                                               // auroraKernels[id_actual_region].pass = auroraKernels[id_actual_region].lastThread/2;                                                
                                               // auroraKernels[id_actual_region].numThreads = auroraKernels[id_actual_region].numThreads  - auroraKernels[id_actual_region].pass;
                                               // if(auroraKernels[id_actual_region].pass == 1)
                                               //         auroraKernels[id_actual_region].state = S3;        
					       // else
	                                       //         auroraKernels[id_actual_region].state = S2;
                                        }
					
				}else{
					auroraKernels[id_actual_region].state = END_THREADS;
					auroraKernels[id_actual_region].bestFreq = TURBO_ON;
                                }
		     		break;
				// else{ 
                                        /* Thread scalability stopped */
                                        /* Find the interval of threads that provided this result. */
                                        /* if the best number of threads so far is equal to the number of cores, then go to.. */
                                //        if(auroraKernels[id_actual_region].bestThread == auroraKernels[id_actual_region].numCores/2){
                                //                auroraKernels[id_actual_region].pass = auroraKernels[id_actual_region].lastThread/2;                                                
                                //                auroraKernels[id_actual_region].numThreads = auroraKernels[id_actual_region].numThreads  - auroraKernels[id_actual_region].pass;
                                //                if(auroraKernels[id_actual_region].pass == 1)
                                //                        auroraKernels[id_actual_region].state = S3;        
        			//		else
		                //                        auroraKernels[id_actual_region].state = S2;
                                //       }else{
                                //                auroraKernels[id_actual_region].pass = auroraKernels[id_actual_region].lastThread/2;                                                
                                //                auroraKernels[id_actual_region].numThreads = auroraKernels[id_actual_region].numThreads  + auroraKernels[id_actual_region].pass;
                                //                if(auroraKernels[id_actual_region].pass == 1)
                                //                        auroraKernels[id_actual_region].state = S3;        
                                //                else
				//			auroraKernels[id_actual_region].state = S2;  
                                //        }
                                  
                                // } 
                           
		
		
		
		
		
                      /*  case S2:        
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
			*/
                      /*  case S3: //The last comparison to define the best number of threads
                                if(result < auroraKernels[id_actual_region].bestResult){
                                        auroraKernels[id_actual_region].bestThread = auroraKernels[id_actual_region].numThreads;
                                        auroraKernels[id_actual_region].state = END_THREADS;
					//ligar turbo core
					//system("echo 1 > /sys/devices/system/cpu/cpufreq/boost");
				        //FILE *turbo = fopen("/sys/devices/system/cpu/cpufreq/boost", "wt");
				        var = 1;
				        fprintf(turbo, "%d", var);
				        //fclose(turbo);
                                }else{
                                        auroraKernels[id_actual_region].state = END_THREADS;
//					system("echo 1 > /sys/devices/system/cpu/cpufreq/boost");
				        //FILE *turbo = fopen("/sys/devices/system/cpu/cpufreq/boost", "wt");
				        var = 1;
				        fprintf(turbo, "%d", var);
				        //fclose(turbo);
					//ligar turbo core
                                }
                                break;
				*/
				
			case END_THREADS:
				if(result < auroraKernels[id_actual_region].bestResult){
					auroraKernels[id_actual_region].bestTurbo = TURBO_ON;
					auroraKernels[id_actual_region].state = END;
					//MATHEUS: executar com game mode (setar cores para a frequencia do game mode)
				}else{
					auroraKernels[id_actual_region].bestTurbo = TURBO_OFF;
					auroraKernels[id_actual_region].state = END;
					//MATHEUS: executar com game mode (setar cores para a frequencia do game mode)
				}
				break;
			case END_TURBO:
				if(result){
					auroraKernels[id_actual_region].bestGame = GAME_ON;
					auroraKernels[id_actual_region].state = END_THREADS;
				}else{
					auroraKernels[id_actual_region].bestGame = GAME_OFF
					auroraKernels[id_actual_region].state = END_THREADS;
				}
				break;
			
		}
	}
	
	turbo = open("/sys/devices/system/cpu/cpufreq/boost", O_WRONLY);
	sprintf(set, "%d", 1);
	write(turbo, set, sizeof(set));
	close(turbo);

}

/* It finalizes the environment of Aurora */
void aurora_destructor(){
        double time = omp_get_wtime() - initGlobalTime;
        id_actual_region = MAX_KERNEL-1;
        double energy = aurora_end_amd_msr();
        printf("AURORA - Execution Time: %.5f seconds\n", time);
        printf("AURORA - Energy: %.5f joules\n",energy);
        printf("AURORA - EDP: %.5f\n",time*energy);
        printf("AURORA - AVG Power Consumption: %.5f\n", energy/time);
}


void aurora_detect_packages() {

	char filename[STRING_BUFFER];
	FILE *fff;
	int package;
	int i;

	for(i=0;i<MAX_PACKAGES;i++) package_map[i]=-1;

	for(i=0;i<MAX_CPUS;i++) {
		sprintf(filename,"/sys/devices/system/cpu/cpu%d/topology/physical_package_id",i);
		fff=fopen(filename,"r");
		if (fff==NULL) break;
		fscanf(fff,"%d",&package);
		fclose(fff);

		if (package_map[package]==-1) {
			auroraTotalPackages++;
			package_map[package]=i;
		}

	}

}

void aurora_start_amd_msr(){
	char msr_filename[STRING_BUFFER];
	int fd;
	sprintf(msr_filename, "/dev/cpu/0/msr");
	fd = open(msr_filename, O_RDONLY);
	if ( fd < 0 ) {
		if ( errno == ENXIO ) {
			fprintf(stderr, "rdmsr: No CPU 0\n");
			exit(2);
		} else if ( errno == EIO ) {
			fprintf(stderr, "rdmsr: CPU 0 doesn't support MSRs\n");
			exit(3);
		} else {
			perror("rdmsr:open");
			fprintf(stderr,"Trying to open %s\n",msr_filename);
			exit(127);
		}
	}
	uint64_t data;
	pread(fd, &data, sizeof data, AMD_MSR_PACKAGE_ENERGY);
	//auroraKernels[id_actual_region].kernelBefore[0] = read_msr(fd, AMD_MSR_PACKAGE_ENERGY);
	auroraKernels[id_actual_region].kernelBefore[0] = (long long) data;
}

double aurora_end_amd_msr(){
	char msr_filename[STRING_BUFFER];
        int fd;
        sprintf(msr_filename, "/dev/cpu/0/msr");
        fd = open(msr_filename, O_RDONLY);
		uint64_t data;
        pread(fd, &data, sizeof data, AMD_MSR_PWR_UNIT);
	int core_energy_units = (long long) data;
	unsigned int energy_unit = (core_energy_units & AMD_ENERGY_UNIT_MASK) >> 8;
	pread(fd, &data, sizeof data, AMD_MSR_PACKAGE_ENERGY);
	auroraKernels[id_actual_region].kernelAfter[0] = (long long) data;
	double result = (auroraKernels[id_actual_region].kernelAfter[0] - auroraKernels[id_actual_region].kernelBefore[0])*pow(0.5,(float)(energy_unit));
	return result;
}
