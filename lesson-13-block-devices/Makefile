CC = gcc
SUBDIRS = $(shell find . -maxdepth 1 -mindepth 1 -type d -printf "%f\n")
#SUBDIRS = $(shell ls -l | awk '/^d/ { print $$9 }')
PROGLIST = test_blk test_seq

all:	prog
	@list='$(SUBDIRS)'; for subdir in $$list; do \
	  echo "=============== making all in $$subdir ================="; \
	  (cd $$subdir && make && cd ../) \
	done;

prog:  $(PROGLIST)

clean:
	@list='$(SUBDIRS)'; for subdir in $$list; do \
	  echo "=============== making clean in $$subdir ==============="; \
	  (cd $$subdir && make clean && cd ../) \
	done;

disclean:
	@list='$(SUBDIRS)'; for subdir in $$list; do \
	  echo "=============== making disclean in $$subdir ==============="; \
	  (cd $$subdir && make disclean && cd ../) \
	done;
	rm -f $(PROGLIST)
