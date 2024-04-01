/* IMPORTANT: EDIT m_class_dispatcher.c INSTEAD OF THIS FILE */
if (x == &pd_objectmaker)
{
    switch (niarg * 16 + nfarg)
    {
    case 0x00 : bonzo = (*(t_fun00)(m->me_fun))(); break;
    case 0x01 : bonzo = (*(t_fun01)(m->me_fun))(ad[0]); break;
    case 0x10 : bonzo = (*(t_fun10)(m->me_fun))(ai[0]); break;
    case 0x02 : bonzo = (*(t_fun02)(m->me_fun))(ad[0], ad[1]); break;
    case 0x11 : bonzo = (*(t_fun11)(m->me_fun))(ai[0], ad[0]); break;
    case 0x20 : bonzo = (*(t_fun20)(m->me_fun))(ai[0], ai[1]); break;
    case 0x03 : bonzo = (*(t_fun03)(m->me_fun))(ad[0], ad[1], ad[2]); break;
    case 0x12 : bonzo = (*(t_fun12)(m->me_fun))(ai[0], ad[0], ad[1]); break;
    case 0x21 : bonzo = (*(t_fun21)(m->me_fun))(ai[0], ai[1], ad[0]); break;
    case 0x30 : bonzo = (*(t_fun30)(m->me_fun))(ai[0], ai[1], ai[2]); break;
    case 0x04 : bonzo = (*(t_fun04)(m->me_fun))(ad[0], ad[1], ad[2], ad[3]); break;
    case 0x13 : bonzo = (*(t_fun13)(m->me_fun))(ai[0], ad[0], ad[1], ad[2]); break;
    case 0x22 : bonzo = (*(t_fun22)(m->me_fun))(ai[0], ai[1], ad[0], ad[1]); break;
    case 0x31 : bonzo = (*(t_fun31)(m->me_fun))(ai[0], ai[1], ai[2], ad[0]); break;
    case 0x40 : bonzo = (*(t_fun40)(m->me_fun))(ai[0], ai[1], ai[2], ai[3]); break;
    case 0x05 : bonzo = (*(t_fun05)(m->me_fun))(ad[0], ad[1], ad[2], ad[3], ad[4]); break;
    case 0x14 : bonzo = (*(t_fun14)(m->me_fun))(ai[0], ad[0], ad[1], ad[2], ad[3]); break;
    case 0x23 : bonzo = (*(t_fun23)(m->me_fun))(ai[0], ai[1], ad[0], ad[1], ad[2]); break;
    case 0x32 : bonzo = (*(t_fun32)(m->me_fun))(ai[0], ai[1], ai[2], ad[0], ad[1]); break;
    case 0x41 : bonzo = (*(t_fun41)(m->me_fun))(ai[0], ai[1], ai[2], ai[3], ad[0]); break;
    case 0x50 : bonzo = (*(t_fun50)(m->me_fun))(ai[0], ai[1], ai[2], ai[3], ai[4]); break;
    default : bonzo = 0;
    }
    pd_this->pd_newest = bonzo;
}
else
{
    switch (niarg * 16 + nfarg)
    {
    case 0x00 : (*(t_vfun00)(m->me_fun))(); break;
    case 0x01 : (*(t_vfun01)(m->me_fun))(ad[0]); break;
    case 0x10 : (*(t_vfun10)(m->me_fun))(ai[0]); break;
    case 0x02 : (*(t_vfun02)(m->me_fun))(ad[0], ad[1]); break;
    case 0x11 : (*(t_vfun11)(m->me_fun))(ai[0], ad[0]); break;
    case 0x20 : (*(t_vfun20)(m->me_fun))(ai[0], ai[1]); break;
    case 0x03 : (*(t_vfun03)(m->me_fun))(ad[0], ad[1], ad[2]); break;
    case 0x12 : (*(t_vfun12)(m->me_fun))(ai[0], ad[0], ad[1]); break;
    case 0x21 : (*(t_vfun21)(m->me_fun))(ai[0], ai[1], ad[0]); break;
    case 0x30 : (*(t_vfun30)(m->me_fun))(ai[0], ai[1], ai[2]); break;
    case 0x04 : (*(t_vfun04)(m->me_fun))(ad[0], ad[1], ad[2], ad[3]); break;
    case 0x13 : (*(t_vfun13)(m->me_fun))(ai[0], ad[0], ad[1], ad[2]); break;
    case 0x22 : (*(t_vfun22)(m->me_fun))(ai[0], ai[1], ad[0], ad[1]); break;
    case 0x31 : (*(t_vfun31)(m->me_fun))(ai[0], ai[1], ai[2], ad[0]); break;
    case 0x40 : (*(t_vfun40)(m->me_fun))(ai[0], ai[1], ai[2], ai[3]); break;
    case 0x05 : (*(t_vfun05)(m->me_fun))(ad[0], ad[1], ad[2], ad[3], ad[4]); break;
    case 0x14 : (*(t_vfun14)(m->me_fun))(ai[0], ad[0], ad[1], ad[2], ad[3]); break;
    case 0x23 : (*(t_vfun23)(m->me_fun))(ai[0], ai[1], ad[0], ad[1], ad[2]); break;
    case 0x32 : (*(t_vfun32)(m->me_fun))(ai[0], ai[1], ai[2], ad[0], ad[1]); break;
    case 0x41 : (*(t_vfun41)(m->me_fun))(ai[0], ai[1], ai[2], ai[3], ad[0]); break;
    case 0x50 : (*(t_vfun50)(m->me_fun))(ai[0], ai[1], ai[2], ai[3], ai[4]); break;
    default : ;
    }
}
