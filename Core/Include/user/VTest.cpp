#include "VTest.h"
#include <iostream>

int user::VTest::AddInt(int a, int b) {
	return (a + b);
}

void user::VTest::Print(std::string value) {
	std::cout << value << std::endl;
}

