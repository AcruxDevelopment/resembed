g++ main.cpp NamingConventionConverter.cpp -o resembed
rm -fr ../test/resources/
./resembed . ../test/resources/ resources.json
