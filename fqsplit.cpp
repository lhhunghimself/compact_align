#include <zlib.h>  
#include <stdio.h>
#include <iostream>
#include <string.h>
#include <sstream>
#include <omp.h>
#include <boost/filesystem.hpp>
namespace fs = boost::filesystem;

extern "C" {
 #include "optparse.h"  
}
bool checkNames(char *line1,char *line2);
int getLines(gzFile fp, char *buffer);
int getLines(gzFile fpR1, gzFile fpR2, char *bufferR1, char *bufferR2, int *sizeR1, int *sizeR2, bool *mismatch);
bool writeLines(FILE *ofp, gzFile ofpgz, char *buffer, int size);
bool writeLines(FILE *ofpR1, FILE *ofpR2, gzFile ofpR1gz, gzFile ofpR2gz, char *bufferR1, char *bufferR2, int sizeR1, int sizeR2);
int finishFile(FILE *ofp, gzFile ofpgz, std::string outputDir, std::string filestem, std::string tempFile,bool compressFlag,int groupNum,bool reopen);
int finishFile(FILE *ofpR1, FILE *ofpR2, gzFile ofpR1gz, gzFile ofgR2gz, std::string outputDir, std::string R1stem, std::string R2stem, std::string R1tempFile, std::string R2tempFile, bool compressFlag,int groupNum, bool reopen);

std::string errmsg="fqsplit h?vzsp:t:o:\n-h -? (display this message)\n-v (Verbose mode)\n-z Compress the output as gzip files\n-s The maximum size of the split file in KB (default, 102400)\n-t <number of threads(1)>\n-o <Output Directory>\n<R1file.1> <R2file.1>..<R1file.N> <R2file.N>\n\nExample:\numisplit -b References/Broad_UMI/barcodes_trugrade_96_set4.dat -o Aligns sample1_R1.fastq.gz sample1_R2.fastq.gz sample2_R1.fastq.gz sample2_R2.fastq.gz\n";
  
int main(int argc, char *argv[]){
    bool compressFlag=0,peFlag=0;
    char verbose=0;
    uint64_t maxSizeKB=102400;  
    char *arg=0,*outputFileName=0;
    struct optparse options;
    int opt;
    optparse_init(&options, argv);
    int nThreads=1;
    std::string outputDir="";
    std::vector<std::string> inputFiles;
 
 //parse flags
    while ((opt = optparse(&options, "h?dvzpt:s:o:")) != -1) {
        switch (opt){
            case 'v':
                verbose=1;
			break;
			case 'p':
                peFlag=1;
            break;			
            case 't':
				nThreads=atoi(options.optarg);
            break;
            case 'z':
                compressFlag=1;
            break; 
            case 's':
                maxSizeKB=std::stoull(options.optarg);
            break;     
			case 'o':
                outputDir=std::string(options.optarg);
			break;  
            case '?':
                fprintf(stderr, "%s parameters are: %s\n", argv[0], errmsg.c_str());
                exit(0);
            break;
            case 'h':
                fprintf(stderr, "%s parameters are: %s\n", argv[0], errmsg.c_str());
                exit(0);
            break; 
        }
    }
	if(outputDir==""){
		fprintf(stderr,"must give an output directory\n");
		exit(EXIT_FAILURE);
	}	
    //parse file arguments
    //if these are paired end R1 willbe followed by R2 arguments
    while ((arg = optparse_arg(&options))){
        inputFiles.push_back(std::string(arg));
	}
	if(verbose){
		//print out the parameters
		fprintf(stderr,"Verbose mode on\n");
		fprintf(stderr,"Output directory is %s\n",outputDir.c_str());
		fprintf(stderr,"Number of threads %d\n",nThreads);	
		fprintf(stderr,"Max filesize %d kB\n",maxSizeKB);	
		int i=0;
		if (peFlag) {
			while (i<inputFiles.size()){
				fprintf(stderr,"R1 file %s\n",inputFiles[i++].c_str());
				if(i == inputFiles.size()){
					fprintf(stderr,"missing corresponding R2 file\n");
					exit(EXIT_FAILURE);
				}
				fprintf(stderr,"R2 file %s\n",inputFiles[i++].c_str());		
			}
	   }		
	}
    fs::create_directory(fs::system_complete(outputDir));

	if (peFlag){
		#pragma omp parallel for num_threads (nThreads) schedule (dynamic)
		for (int i=0;i<inputFiles.size();i+=2){
			const uint64_t maxSize=1024*maxSizeKB;
			const int tid=omp_get_thread_num();
			gzFile fpR1=0, fpR2=0, ofpR1gz=0, ofpR2gz=0;
			FILE *ofpR1=0,*ofpR2=0 ; 
			fpR1 = gzopen(inputFiles[i].c_str(), "r");
			if(fpR1 == Z_NULL){
				fprintf(stderr,"unable to open %s\n",inputFiles[i].c_str());
				exit(EXIT_FAILURE);
			}
			fpR2 = gzopen(inputFiles[i+1].c_str(), "r");	
			if(fpR2 == Z_NULL){
				fprintf(stderr,"unable to open %s\n",inputFiles[i+1].c_str());
				exit(EXIT_FAILURE);
			}
  
			fs::path R1(inputFiles[i]);
			fs::path R2(inputFiles[i+1]);
			std::string R1stem,R2stem;
			R1=R1.stem();
			R2=R2.stem();
			while (R1.extension().string() == ".gz" || R1.extension().string() == ".fastq" || R1.extension().string() == ".fq")   
				R1=R1.stem();
			while (R2.extension().string() == ".gz" || R2.extension().string() == ".fastq" || R2.extension().string() == ".fq")   
				R2=R2.stem();   
			R1stem=R1.stem().string();
			R2stem=R2.stem().string();

			std::string R1tempFile=outputDir+'/'+R1stem+".temp";
			std::string R2tempFile=outputDir+'/'+R2stem+".temp";

			char linesR1[4096],linesR2[4096];
			bool mismatch=0;
			int groupSize=0,totalSize=0,sizeR1=0,sizeR2=0,groupNum=0,nameMismatch=0;
			if (compressFlag){
				ofpR1gz=gzopen(R1tempFile.c_str(), "wb");
				ofpR2gz=gzopen(R2tempFile.c_str(), "wb");
			}
			else{
				ofpR1=fopen(R1tempFile.c_str(), "w");
				ofpR2=fopen(R2tempFile.c_str(), "w");				
			}
			while (groupSize=getLines(fpR1,fpR2,linesR1,linesR2,&sizeR1,&sizeR2,&mismatch)){
				const bool reopen=1;
				if (mismatch){
					nameMismatch++;
					if(verbose)fprintf(stderr,"Warning - mismatch of names in R1 and R2\n");
				}
		 		if (groupSize + totalSize > maxSize){
					groupNum=finishFile(ofpR1,ofpR2,ofpR1gz, ofpR2gz,outputDir,R1stem,R2stem,R1tempFile,R2tempFile,compressFlag,groupNum,reopen);
					totalSize=0;
				}
				totalSize+=groupSize;
				writeLines(ofpR1,ofpR2,ofpR1gz,ofpR2gz,linesR1,linesR2,sizeR1,sizeR2);
            }
           const bool reopen=0;
           finishFile(ofpR1,ofpR2,ofpR1gz, ofpR2gz,outputDir,R1stem,R2stem,R1tempFile,R2tempFile,compressFlag,groupNum, reopen); 
		}
	}
	else{
		#pragma omp parallel for num_threads (nThreads) schedule (dynamic)
		for (int i=0;i<inputFiles.size();i++){
			const uint64_t maxSize=1024*maxSizeKB;
			const int tid=omp_get_thread_num();
			gzFile fp=0, ofpgz=0;
			FILE *ofp=0; 
			fp = gzopen(inputFiles[i].c_str(), "r");
			if(fp == Z_NULL){
				fprintf(stderr,"unable to open %s\n",inputFiles[i].c_str());
				exit(EXIT_FAILURE);
			}
			fs::path file(inputFiles[i]);
			while (file.extension().string() == ".gz" || file.extension().string() == ".fastq" || file.extension().string() == ".fq")   
				file=file.stem();
   
			std::string filestem=file.stem().string();
			std::string tempFile=outputDir+'/'+filestem+".temp";

			char lines[4096];
			int groupSize=0,totalSize=0,size,groupNum=0;
			if (compressFlag) ofpgz=gzopen(tempFile.c_str(), "wb");
			else ofp=fopen(tempFile.c_str(), "w");
			while (size=getLines(fp,lines)){
				const bool reopen=1;
		 		if (size + totalSize > maxSize){
					groupNum=finishFile(ofp,ofpgz,outputDir,filestem,tempFile,compressFlag,groupNum,reopen);
					totalSize=0;
				}
				totalSize+=size;
				writeLines(ofp,ofpgz,lines,size);
            }
           const bool reopen=0;
           finishFile(ofp,ofpgz,outputDir,filestem,tempFile,compressFlag,groupNum,reopen);
		}
		
	}
    return 0;  
}
bool writeLines(FILE *ofp, gzFile ofpgz, char *buffer, int size){
	if (ofpgz){
		if (!gzwrite(ofpgz,buffer,size)) return 1;
		return 0;
	}
	if (ofp){
		if (!fwrite(buffer,size,1,ofp)) return 1;
		return 0;
	}
	return 1;
}
bool writeLines(FILE *ofpR1, FILE *ofpR2, gzFile ofpR1gz, gzFile ofpR2gz, char *bufferR1, char *bufferR2, int sizeR1, int sizeR2){
	if (ofpR1gz && ofpR2gz){
		if (!gzwrite(ofpR1gz,bufferR1,sizeR1)) return 1;
		if (!gzwrite(ofpR2gz,bufferR2,sizeR2)) return 1;
		return 0;
	}
	if (ofpR1 && ofpR2){
		if (!fwrite(bufferR1,sizeR1,1,ofpR1)) return 1;
		if (!fwrite(bufferR2,sizeR2,1,ofpR2)) return 1;
		return 0;
	}
	return 1;
}
int finishFile(FILE *ofp, gzFile ofpgz, std::string outputDir, std::string filestem, std::string tempFile,bool compressFlag,int groupNum,bool reopen){
	if (compressFlag) gzclose(ofpgz);
	else fclose(ofp);
	//change name of temp file to permanent output file
	std::string outputFile=outputDir+"/"+filestem+"_"+std::to_string(groupNum)+".fastq";
	if (compressFlag) outputFile+=".gz";
	fs::rename(tempFile,outputFile);
	if (reopen){
		if (compressFlag) ofpgz=gzopen(tempFile.c_str(), "wb");
		else ofp=fopen(tempFile.c_str(), "w");
	}					
	return groupNum+1;
}
int finishFile(FILE *ofpR1, FILE *ofpR2, gzFile ofpR1gz, gzFile ofpR2gz, std::string outputDir, std::string R1stem, std::string R2stem, std::string R1tempFile, std::string R2tempFile, bool compressFlag,int groupNum,bool reopen){
	if (compressFlag){
		gzclose(ofpR1gz);
		gzclose(ofpR2gz);
	}
	else{
		fclose(ofpR1);
		fclose(ofpR2);
	}
	//change name of temp file to permanent output file
	std::string R1outputFile=outputDir+"/"+R1stem+"_"+std::to_string(groupNum)+".fastq";
	std::string R2outputFile=outputDir+"/"+R2stem+"_"+std::to_string(groupNum)+".fastq";
	if (compressFlag){
		R1outputFile+=".gz";
		R2outputFile+=".gz";
	}
	fs::rename(R1tempFile,R1outputFile);
	fs::rename(R2tempFile,R2outputFile);
	if (reopen){
		if (compressFlag){
			ofpR1gz=gzopen(R1tempFile.c_str(), "wb");
			ofpR2gz=gzopen(R2tempFile.c_str(), "wb");
		}
		else{
			ofpR1=fopen(R1tempFile.c_str(), "w");
			ofpR2=fopen(R2tempFile.c_str(), "w");				
		}
	}					
	return groupNum+1;
}

int getLines(gzFile fp, char *buffer){
    const int lineSize=1024;
    int lengthR1=0,size=0;
    char *mybuffer=buffer;   
    for (int i=0;i<4;i++){
		if (!gzgets(fp, mybuffer, lineSize)) return 0;
		mybuffer+=strlen(mybuffer);
    }
	return mybuffer-buffer;    
}
int getLines(gzFile fpR1, gzFile fpR2, char *bufferR1, char *bufferR2, int *sizeR1, int *sizeR2, bool *mismatch){
    const int lineSize=1024;
    int lengthR1=0;
    int lengthR2=0;
    int size=0;
    char *mybufferR1=bufferR1,*mybufferR2=bufferR2;
    if (!gzgets(fpR1, mybufferR1, lineSize) || !gzgets(fpR2, mybufferR2, lineSize)) return 0;
    *mismatch=checkNames(bufferR1,bufferR2);		
    lengthR1=strlen(mybufferR1);
	lengthR2=strlen(mybufferR2);
	mybufferR1+=lengthR1;
	mybufferR2+=lengthR2;
	if (*mismatch)fprintf(stderr,"warning mismatch %s %s",bufferR1,bufferR2);    
    for (int i=0;i<3;i++){
		if (!gzgets(fpR1, mybufferR1, lineSize) || !gzgets(fpR2, mybufferR2, lineSize)) return 0;
		lengthR1=strlen(mybufferR1);
		lengthR2=strlen(mybufferR2);
		mybufferR1+=lengthR1;
		mybufferR2+=lengthR2;
    }
	size=mybufferR1-bufferR1+mybufferR2-bufferR2;
	*sizeR1=mybufferR1-bufferR1;
	*sizeR2=mybufferR2-bufferR2;
    return size;    
}
bool checkNames(char *line1,char *line2){
    char *a=line1,*b=line2;
    while(*a && *b){ 
        if (*a != *b){
			return 1;
		}
        a++;
        b++;
    }
    return 0;    
}

