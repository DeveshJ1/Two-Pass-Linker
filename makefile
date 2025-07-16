# Target to build the linker
linker: linker.cpp
	# Use g++ to compile with debugging info
	g++ -g linker.cpp -o linker

# Clean target to remove the executable and backup files
clean:
	# Remove the linker executable and backup files
	rm -f linker *~
