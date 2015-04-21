
#include "../storage/HerpHash.h"
#include "../mmap_filesystem/HerpmapWriter.h"
#include "../mmap_filesystem/HerpmapReader.h"

void write( std::string prefix , int limit ) {
	Storage::Filesystem fs("data.db");
    File f = fs.open_file( "merp" );

    Storage::HerpHash<std::string,int> merp;
    for( int value = 0 ; value < limit ; ++value ) {
        std::string key( prefix + std::to_string( value ) );
        merp.put( key , value );
    }
    Storage::HerpmapWriter<int> writer( f , &fs );
    writer.write( merp );
}

void read( std::string prefix , int limit ) {
	Storage::Filesystem fs("data.db");
    File f = fs.open_file( "merp" );
    Storage::HerpmapReader<int> reader( f , &fs );

    Storage::HerpHash<std::string,int> merp = reader.read(); ;
    for( int value = 0 ; value < limit ; ++value ) {
        std::string key( prefix + std::to_string( value ) );
        int test = merp.get( key );
        Assert( "Junk?" , test , value , test == value );
    }
}

int main(int argc, char *argv[]) {
    int limit = 100;
    if( argc == 2 ) {
        limit = atoi( argv[1] );
    }
    limit = std::max( limit , 10 );
    write( "herp" , limit );
    read( "herp" , limit );
    return 0;
}
