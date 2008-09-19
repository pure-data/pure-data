#! /bin/sh
for i  in `find . -name "*.[ch]" -o -name "*.tk"`  ; do
expand $i > /tmp/expanded-src
if ( ! cmp -s $i /tmp/expanded-src ) ; then 
    echo detabbing: $i
    cp /tmp/expanded-src $i
fi
done
