#! /bin/sh
for i  in `find . -name "*.[ch]" -o -name "*.tk"`  ; do
expand $i > /tmp/foo
if ( ! cmp -s $i /tmp/foo ) ; then 
    echo detabbing: $i
    cp /tmp/foo $i
fi
done
