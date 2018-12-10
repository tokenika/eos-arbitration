
all: escrow.wasm escrow

escrow.wasm: escrow.cpp
	eosio-cpp -o escrow.wasm escrow.cpp --abigen

clean:
	rm -fr escrow.wasm escrow.abi *~
