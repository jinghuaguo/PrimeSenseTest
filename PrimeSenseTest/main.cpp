#include "primesensetest.h"
#include <QtWidgets/QApplication>
#include "Utils/console.h"

int main(int argc, char *argv[])
{
	createConsole();
	QApplication a(argc, argv);
	PrimeSenseTest w;
	w.show();
	return a.exec();
}
