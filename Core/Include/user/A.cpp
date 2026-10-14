#include "A.h"
#include <iostream>

void user::A::EventGraph() {
	switch (a) {
	case 0:
		break;
	case 1:
		a = (a + 1);
		std::cout << a << std::endl;
		break;
	default:
		break;
	}
}

