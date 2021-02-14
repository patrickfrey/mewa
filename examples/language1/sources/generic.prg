extern "C" procedure printf( const byte^ fmt ...);
extern "C" function malloc byte^( long);

generic class List<ELEMENT,ALLOCATOR = std::allocator<ELEMENT> >
{
}

