release checklist
    version string in s_main.c
    test OSS and ALSA
    release notes
    ./make-release 0.35-0  or 0.35-test11, etc
    rsync -n -avzl --delete /home/msp/pd/doc/1.manual/ \
	crca.ucsd.edu:public_html/Pd_documentation

    update rpmspec version number
    as root:
    rpmbuild -ba rpmspec
    rpmbuild -bb rpmspec-alsa
    check size of compressed files:
    	/usr/src/redhat/SRPMS/pd-0.36-0.src.rpm
    	/usr/src/redhat/RPMS/i386/pd-0.36-0.i386.rpm 
    	/usr/src/redhat/RPMS/i386/pd-alsa-0.36-0.i386.rpm
    copy from /usr/src/redhat/RPMS/i386 and /usr/src/redhat/SRPMS
    scp ~/pd/doc/1.manual/* felix:public_html/Pd_documentation/
    put on web page and on ftp://sdfelix.ucsd.edu/pub/msp
    copy README.txt to web page
    mail release notice from ../attic

announce to:
    pd-announce@iem.kug.ac.at
    http://crca.ucsd.edu/~msp/software.html

less formal:
    http://crca.ucsd.edu/~msp/Software
    http://crca.ucsd.edu/~msp/Software/pd-033TEST7-linux.tar.gz
