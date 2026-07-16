g++ \
	src/main.cpp \
	src/NamingConventionConverter.cpp \
	-o resembed

rm -fr ../test/Resources/Embeds

./resembed resources ../test/Resources/Embeds resources/resources.json
