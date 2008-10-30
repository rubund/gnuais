#include <stdio.h>



int main(int argc, char **argv){

	FILE *in, *out;

short tmp;
in = fopen(argv[1],"r");
out = fopen(argv[2],"w");
int fra=0, til=1000000000;
if(argc >= 4)
	 fra = atoi(argv[3]);
if (argc == 5)
	 til = atoi(argv[4]);
printf("from %d, to %d\n",fra,til);
int i=0;
while(fscanf(in,"%d",&tmp)!=EOF){
if (i>=fra)
	fwrite(&tmp,2,1,out);
if (i> til)
	break;
i++;
}


fclose(in);
fclose(out);



}

