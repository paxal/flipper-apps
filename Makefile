.PHONY: test
test:
	cc -Wall -Wextra -I csc_debug/helpers -o /tmp/test_csc_parse \
		tests/test_csc_parse.c csc_debug/helpers/csc_parse.c -lm
	/tmp/test_csc_parse
