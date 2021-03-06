#include "poc.h"

void poc_Setup(BLS &bls, Player &P)
{
    bls.dstb_keygen(P);
}

void poc_EnphemKey(G2_Affine_Coordinates &ac, BLS &bls, const string &msg,
                   int online_num, Player &P, Config_Info &CI)
{
    Processor Proc(online_num, P.nplayers(), P);
    bls.dstb_sign(ac, msg, Proc, online_num, P, CI.OCD, CI.machine);
}
/*
void poc_EphemKey(G2_Affine_Coordinates &ac, BLS &bls, const string msg,
                  Processor &Proc, int online_num, Player &P,
                  offline_control_data &OCD, Machine &machine)
{
    bls.dstb_sign(ac, msg, Proc, online_num, P, OCD, machine);
}
*/

int poc_GenProof(const vector<Share> &keys, const vector<gfp> &msg,
                 int online_num, Player &P, Config_Info &CI)
{
    if (keys.size() != 2)
    {
        throw bad_value();
    }

    Processor Proc(online_num, P.nplayers(), P);
    Share uhf_out;
    int res;
    OnlineOp online_op(Proc, online_num, P, CI.OCD, CI.machine);
    online_op.uhf(uhf_out, keys[0], msg, CHUNK_NUM);
    res = online_op.legendre_prf(keys[1], uhf_out);

    cout << "used triple: " << online_op.UT.UsedTriples << endl;
    cout << "used square: " << online_op.UT.UsedSquares << endl;
    cout << "used bit: " << online_op.UT.UsedBit << endl;
    cout << "used input mask: " << online_op.UT.UsedInputMask << endl;
    return res;
}

/*
int poc_GenProof(const vector<Share> keys, const vector<gfp> msg,
                 Processor &Proc, int online_num, Player &P,
                 offline_control_data &OCD, Machine &machine)
{
    if (keys.size() != 2)
    {
        throw bad_value();
    }

    Share uhf_out;
    int res;
    OnlineOp online_op(Proc, online_num, P, OCD, machine);
    online_op.uhf(uhf_out, keys[0], msg, CHUNK_NUM);
    res = online_op.legendre_prf(keys[1], uhf_out);
    return res;
}
*/