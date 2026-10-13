#include <stdio.h>

int* getArray(){
	int arr[5] = {1,2,3,4,5};
	return arr;
}

int main(){

	int* p = getArray();

	for(int i=0;i<5;i++){
		printf("%d ", p[i]);
	}

	return 0;
}