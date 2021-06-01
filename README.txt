Hot to build and run:

0. Install boost with version not less than 1_74.
1. Set environment variables: BOOST_DIR_INCLUDES to boost/include, BOOST_DIR_LIB to boost/stage/lib.
2. Run generate.bat
3. Build solution in Compile folder.
4. Run program from command line in Compile/build/bin/Release.

Notes:

Task 1. DnsResolver
	Took much code from Win API example and didn't have time to refactor it enough.
	Because of that code had stuff string/wstring. Therefore in some places have to convert.
	Inject boost::log and some debug stuff into the code without surrounding these parts by Debug defines because didn't have time.
	Because of boost::log task works slower but I have to use this in order to check things inside the algorithm.
	Code could be further optimized including use of allocators for dynamically allocated locals and possibly use of explicit multithreading inside DnsResolver (but not sure in it. Tests are necessary.)
	
Task 2. Sets intersection with repetitions
	Implemented two variants of the algorithm because didn't know what will be faster.
