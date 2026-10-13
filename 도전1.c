
#include <stdio.h>

int main(){
	int p = 127;
	int q = 255;

	int* bbq = &p;
	int* kfc = &q;


	int* temp = bbq;
	bbq = kfc;
	kfc = temp;

	printf("bbq: %d\n", *bbq);
	printf("kfc: %d\n", *kfc);

	return 0;
}