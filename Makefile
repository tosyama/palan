all:
	@mkdir -p bin
	@$(MAKE) -C src

package:
	-apt-get -y install libboost-dev
	-apt-get -y install libboost-program-options-dev
	-apt-get -y install bison
	-apt-get -y install flex
	-curl -o src/test/catch.hpp https://raw.githubusercontent.com/catchorg/Catch2/master/single_include/catch2/catch.hpp

coverage:
	@$(MAKE) -C src coverage

