# Expected to be run from the repository root

NBITS=$1

OUTDIR=data/

# Note: genrsa is superceded by genpkey, but the latter supports only >256-bit
openssl genrsa -out $OUTDIR/private$NBITS.pem -3 $NBITS
openssl rsa -out $OUTDIR/private$NBITS.txt -text -in $OUTDIR/private$NBITS.pem

scripts/format-key.py $OUTDIR/private$NBITS.txt $OUTDIR/key$NBITS.txt
