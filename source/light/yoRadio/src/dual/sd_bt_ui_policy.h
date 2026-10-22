#pragma once
namespace ibtsdui {
inline bool classic(bool transmitting,bool local,bool player){return transmitting&&local&&player;}
inline bool due(bool active,unsigned elapsed){return !active||elapsed>=50;}
}
