release checklist
    update Makefile.am doc list: cd pd;
        find doc -type f | sort | awk '{print "    ", $1, "\\"}'
    version string in ../src/m_pd.h ../configure.ac ../src/pd.rc
    release notes ../doc/1.manual/x5.htm
    copyright date in ../README.txt
    test compilation on linux/msw/mac
    git commit
    ./make-release 0.35-0  or 0.35-test11, etc
    ... compile on windows/Mac 
    git tag (to see existing tags)
    git tag 0.43-3test1 (e.g.)
    git push --mirror
    copy from ~/pd/dist to ~/bis/lib/public_html/Software/
    rsync -avzl --delete ~/pd/doc/1.manual/ \
        ~/bis/lib/public_html/Pd_documentation/
    chmod -R g-w ~/bis/lib/public_html/Pd_documentation/
    cp -a ~/pd/README.txt ~/bis/lib/public_html/Software/pd-README.txt
    (cd /home/msp/bis/lib/public_html/Software; htmldir.perl .)
    nedit-client /home/msp/bis/lib/public_html/software.htm
    copy-out.sh +

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
