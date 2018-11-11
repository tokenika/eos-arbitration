
all: dep.wasm dep

dep.wasm: dep.cpp
	eosio-cpp -o dep.wasm dep.cpp --abigen

dep:
	cleos set contract dep ../dep --abi dep.abi -p dep@active #| grep -ve "transaction executed locally, but may not be confirmed by the network yet" > /dev/stderr

clean:
	rm -fr dep.wasm dep.abi *~
