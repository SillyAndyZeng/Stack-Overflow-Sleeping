#include <iostream>
#include <vector>
using namespace std;
// 在此处补充你的代码
#include <algorithm>

template<class T, class Comp = greater<int>>
class getWanted{
    int value;
public:
    getWanted(T i1, T i2){
        vector<int> temp(i1, i2);
        sort(temp.begin(), temp.end(), Comp());
        value = temp.back();
    }

    template<typename Func>
    getWanted(T i1, T i2, Func func){
        vector<typename iterator_traits<T>::value_type> temp(i1, i2);
        sort(temp.begin(), temp.end(), func); 
        value = temp.back();
    }
    friend ostream& operator<<(ostream &os, const getWanted<T, Comp>& gw){
        os << gw.value;
        return os;
    }
};

struct myComp {
    //比个位排序，个位小的在前
	bool operator ()(int a,int b) {
		return a % 10 < b % 10;
	}
};
int main()
{
	int n;
	cin >> n;
	vector<int> a,b,c;
	for(int i=0;i<n;++i) {
		int x;
		cin >> x;
		a.push_back(x);
	}
	int cmd;
	while( cin >> cmd ) {
		switch(cmd) {
			case 0:
				cout << getWanted<vector<int>::iterator,less<int>>(a.begin(),a.end()) << endl;
				break;
			case 1:
				cout << getWanted<vector<int>::iterator>(a.begin(),a.end(), less<int>()) << endl;	
				break;
			case 2:
				cout << getWanted<vector<int>::iterator>(a.begin(),a.end(),myComp()) << endl;
				break;
			case 3:
				cout << getWanted<vector<int>::iterator>(a.begin(),a.end()) << endl; 
				break;
		}
	}
	return 0;
}