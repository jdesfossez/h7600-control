Hand-written test data, not taken from Eventide's files: `test_db.txt` is a tiny module database
in the 0C record format with made-up, minimal fields, and `test_gain.sig` is a sigfile that uses
it. Tests that need the real database or factory sigfiles read them from `H7600_SIGDBASE` and
`H7600_SIGFILES_DIR` (default `~/Documents/h7600/sigfiles`) and skip when absent.
