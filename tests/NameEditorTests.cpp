#include "NameEditor.h"
#include <cstdlib>
#include <iostream>
void Check(bool ok) {if(!ok){std::cerr<<"Name editor regression failed\n";std::exit(1);}}
#define assert(value) Check(value)
int main() {
    Wheel::NameEditor e;e.InsertUtf16(u"旅人\U0001f6e1甲");
    assert(e.text.size()==13 && e.cursor==13);
    e.Move(-1,false);assert(e.cursor==10);
    e.Backspace();assert(e.text=="旅人甲" && e.cursor==6);
    e.Move(-1,true);assert(e.Selection()=="人");
    e.Insert(0x5149);assert(e.text=="旅光甲");
    e.Place(0);e.Delete();assert(e.text=="光甲");
    e.SelectAll();e.InsertUtf16(u"新预设");assert(e.text=="新预设");
    e.Move(-1,true);e.Move(-1,true);assert(e.Selection()=="预设");
    e.Move(-1,false);assert(e.cursor==3 && !e.Selected());
    e.Place(0);e.Backspace();assert(e.text=="新预设");
    e.Place(e.text.size());e.Delete();assert(e.text=="新预设");
    e.Set(std::string(127,'a'));assert(!e.Insert(0x4e2d));assert(e.text.size()==127);
    e.SelectAll();assert(!e.Insert(0xd800) && e.Selected());
    e.InsertUtf16(u"\n\r\t\x007f\xd800" );assert(e.text.size()==127);
    assert(e.Insert('x') && e.text=="x");
    e.InsertUtf16(u"\xd800\xdc00");assert(e.text.size()==5);
    std::cout<<"Unicode insertion, selection, caret, deletion, bounds and invalid scalar checks passed\n";
}
