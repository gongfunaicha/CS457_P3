all: router manager

router: router.o
	g++ -Wall -std=c++11 -pthread router.o -o router

manager: manager.o cost_info.o
	g++ -Wall -std=c++11 manager.o cost_info.o -o manager

cost_info.o: cost_info.h cost_info.cpp
	g++ -Wall -std=c++11 -c cost_info.cpp

manager.o: cost_info.h manager.cpp
	g++ -Wall -std=c++11 -c manager.cpp

router.o:  router.cpp
	g++ -Wall -std=c++11 -c router.cpp

clean:
	rm -f *.o manager router
