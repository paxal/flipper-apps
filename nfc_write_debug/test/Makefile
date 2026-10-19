.PHONY: check
check:
	$(CC) -Wall -Wextra -std=c11 -I.. -o /tmp/test_nfc_write_diff \
		test_nfc_write_diff.c ../helpers/nfc_write_diff.c
	/tmp/test_nfc_write_diff
