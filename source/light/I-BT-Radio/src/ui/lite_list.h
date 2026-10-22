#pragma once
// Native Light list geometry is shared by painting and hit testing.
namespace litelist {
constexpr unsigned rows=7;
constexpr int top=56,step=23;
constexpr unsigned first(unsigned selected,unsigned total){
 if(!total)return 0;
 unsigned n=selected>3?selected-3:1;
 const unsigned last=total>rows?total-rows+1:1;
 return n>last?last:n;
}
constexpr int row(int x,int y){return x>=0&&x<320&&y>=top&&y<top+int(rows)*step?(y-top)/step:-1;}
constexpr unsigned station(unsigned first,unsigned total,int row){
 return first&&row>=0&&row<int(rows)&&first+unsigned(row)<=total?first+unsigned(row):0;
}
}
