zip -r pd-0.41-0test09.msw.zip pd

makesfx /zip=pd-0.41-0test09.msw.zip /sfx=pd-0.41-0test09.msw.exe /title=Pd /website=crca.ucsd.edu/~msp /defaultpath=$programfiles$

pscp pd-0.41-0test09.msw.zip pd-0.41-0test09.msw.exe msp@crca.ucsd.edu:public_html/Software
