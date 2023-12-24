CXX = g++
CXXFLAGS = -std=c++17 -fsanitize=address -Wall -Wextra -pedantic -I/usr/local/include
LIBS = -lcurl -lgumbo -lcassandra
HEADERS = $(wildcard *.h)
EXECUTABLE = sitemap
SCRAPPER_EXECUTABLE = scrapper

all: $(EXECUTABLE) $(SCRAPPER_EXECUTABLE)


$(EXECUTABLE): main.cpp $(HEADERS)
	$(CXX) $(CXXFLAGS) main.cpp -o $(EXECUTABLE) $(LIBS)

$(SCRAPPER_EXECUTABLE): scrapper.cpp $(HEADERS)
	$(CXX) $(CXXFLAGS) scrapper.cpp -o $(SCRAPPER_EXECUTABLE) $(LIBS)
clean:
	rm -f $(EXECUTABLE) $(SCRAPPER_EXECUTABLE)
