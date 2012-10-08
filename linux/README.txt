release checklist
    version string in s_main.c
    release notes ../doc/1.manual/x5.htm
    copyright date in ../README.txt
    pd version number in ../src/pd.rc
    git tag (to see existing tags)
    git tag 0.43-3test1 (e.g.)
    git push --tags
    ./make-release 0.35-0  or 0.35-test11, etc
    ... compile on windows/Mac 
    ... copy to ~/bis/lib/public_html/Software/
    rsync -avzl --delete ~/pd/doc/1.manual/ \
	~/bis/lib/public_html/Pd_documentation/
    cp -a ~/pd/README.txt ~/bis/lib/public_html/Software/pd-README.txt
    (cd /home/msp/bis/lib/public_html/Software; htmldir.perl .)
    edit /home/msp/bis/lib/public_html/software.htm
    rsync  -avzl --delete /home/msp/bis/lib/public_html/ crca:public_html/
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
