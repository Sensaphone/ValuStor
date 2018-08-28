SUBDIRS   = php7 python2.7 perl5

default: all

subdirs:
	for dir in $(SUBDIRS); do \
		$(MAKE) -C $$dir; \
	done

all: subdirs
