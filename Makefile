all:
	@mkdir -p bin
	@$(MAKE) -C src
	@cp src/pac bin/
	@cp src/ast/pat bin/
clean:
	@rm -f bin/*
	@$(MAKE) -C src clean
package:
	-apt-get -y install libboost-dev
	-apt-get -y install libboost-program-options-dev
	-apt-get -y install bison
	-apt-get -y install flex
	-curl -o src/test/catch.hpp https://raw.githubusercontent.com/catchorg/Catch2/v2.x/single_include/catch2/catch.hpp
coverage:
	@$(MAKE) -C src coverage
release: clean
	@mkdir -p bin
	@$(MAKE) -C src release
	@cp src/pac bin/
	@cp src/ast/pat bin/

