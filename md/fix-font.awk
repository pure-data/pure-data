BEGIN  {last = -10}
/DejaVuSansMono-Bold findfont 9/  {
        last=NR
        $1 = "/Courier-Bold"
        $3 = 11.5
    }
    {
        if (NR == last+2) {
            $2 = $2+0
            $3 = $3-2
        }
        print
    }
