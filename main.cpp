#include <stdio.h>
#include <iostream>
#include <vector>
#include <string>

using namespace std;

int main()
{
    vector<string> msg {"Hello", "C++", "World", "from", "VS Code", "and the C++ extension!"};
    msg.push_back("bismillah");
    for (const string& word : msg)
    {
        cout << word << " ";
    }
    cout << endl;
}
// int main(){
//     printf("bismillah...\n");
//     char text[100];
//     scanf("%s", text);
//     printf("You Have entered : %s\n", text);
// }