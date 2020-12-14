static %A1 global_1 = 1;

%A1 setglobal( %A1 a )
{
    global_1 = a;
    return global_1;
}
