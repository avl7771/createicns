createicns: createicns.c

readicns: readicns.c

.PHONY: clean
clean:
	-rm -f createicns readicns $(objects)
