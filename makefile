all: router manager

router: router.o cost_info.o dijkstra.o
	g++ -Wall -std=c++11 -pthread router.o cost_info.o dijkstra.o -o router

manager: manager.o cost_info.o dijkstra.o
	g++ -Wall -std=c++11 manager.o cost_info.o dijkstra.o -o manager

cost_info.o: cost_info.h cost_info.cpp
	g++ -Wall -std=c++11 -c cost_info.cpp

manager.o: cost_info.h manager.cpp
	g++ -Wall -std=c++11 -c manager.cpp

router.o:  router.cpp cost_info.h dijkstra.h
	g++ -Wall -std=c++11 -c router.cpp

dijkstra.o: dijkstra.h dijkstra.cpp
	g++ -Wall -std=c++11 -c dijkstra.cpp

clean:
	rm -f *.o manager router
