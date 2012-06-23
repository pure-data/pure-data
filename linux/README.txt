release checklist
    version string in s_main.c
    release notes
    git tag (to see existing tags)
    git commit and push
    ./make-release 0.35-0  or 0.35-test11, etc
    rsync -avzl --delete ~/pd/doc/1.manual/ \
	~/bis/lib/public_html/Pd_documentation/
    cp -a ~/pd/README.txt ~/bis/lib/public_html/Software/pd-README.txt
    (cd /home/msp/bis/lib/public_html/Software; htmldir.perl .)
    rsync  -avzl --delete /home/msp/bis/lib/public_html/ crca:public_html/
    edit /home/msp/bis/lib/public_html/software.htm
    repeat rsync
    mail release notice from /home/msp/pd/attic/pd-announce

rpm building (inactive)
    update rpmspec version number
    as root:
    rpmbuild -ba rpmspec
    rpmbuild -bb rpmspec-alsa
    check size of compressed files:
    	/usr/src/redhat/SRPMS/pd-0.36-0.src.rpm
    	/usr/src/redhat/RPMS/i386/pd-0.36-0.i386.rpm 
    	/usr/src/redhat/RPMS/i386/pd-alsa-0.36-0.i386.rpm
    copy from /usr/src/redhat/RPMS/i386 and /usr/src/redhat/SRPMS
