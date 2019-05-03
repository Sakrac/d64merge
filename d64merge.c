#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define D64_SIZE 174848
#define D64_FILE_ENTRY_SIZE 32
#define D64_MAX_FILES 256
typedef unsigned char u8;

static const int TrackOffsets[] = {
	0x00000, 0x01500, 0x02A00, 0x03F00, 0x05400, 0x06900, 0x07E00,
	0x09300, 0x0A800, 0x0BD00, 0x0D200, 0x0E700, 0x0FC00, 0x11100,
	0x12600, 0x13B00, 0x15000, 0x16500, 0x17800, 0x18B00, 0x19E00,
	0x1B100, 0x1C400, 0x1D700, 0x1EA00, 0x1FC00, 0x20E00, 0x22000,
	0x23200, 0x24400, 0x25600, 0x26700, 0x27800, 0x28900, 0x29A00,
	0x2AB00, 0x2BC00, 0x2CD00, 0x2DE00, 0x2EF00
};

static const int SectorsPerTrack[] = {
	21, 21, 21, 21, 21, 21, 21, 21,
	21, 21, 21, 21, 21, 21, 21, 21,
	21,
	19, 19, 19, 19, 19, 19, 19,
	18, 18, 18, 18, 18, 18,
	17, 17, 17, 17, 17, 17, 17, 17,
	17, 17
};

static const char* FileTypes[] = {
	"DEL", "SEQ", "PRG", "USR", "REL", "???", "???", "???"
};

int d64Sector( u8 track, u8 sector )
{
	return TrackOffsets[ track-1 ] + 256 * (int)sector;
}

void d64FileChain( u8* data, u8 track, u8 sector )
{
	while( track && track <= 40 && sector < SectorsPerTrack[ track-1 ] ) {
		u8* block = data + d64Sector( track, sector );
		printf( "%d, %d", track, sector );
		if( block[ 0 ] ) { printf( " -> " ); }
		else { printf( "\n" ); }
		track = block[ 0 ];
		sector = block[ 1 ];
	}
}

void printFile( u8* file )
{
	u8 type = file[ 2 ];	
	char name[ 17 ];
	for( int n = 0; n<16; ++n ) { name[ n ] = file[ n + 5 ] == 0xa0 ? '.' : file[ n + 5 ]; }
	name[ 16 ] = 0;
	printf( "\"%s\" %s\n", name, FileTypes[ type & 7 ] );
}

int fileCount( u8* d64, int list, int chain )
{
	int numFiles = 0;
	u8* curr = d64 + d64Sector( 18, 0 );
	while(curr[0]) {
		curr = d64 + d64Sector( curr[0], curr[1] );
		u8* file = curr;
		while( ( file - curr ) < 256 ) {
			if( list ) { printFile( file ); }
			if( file[2] ) { // type == 0 => "deleted"
				if( chain && (file[2]&7) ) { d64FileChain( d64, file[3], file[4] ); }
				++numFiles;
			}
			file += D64_FILE_ENTRY_SIZE;
		}
	}
	return numFiles;
}

void d64RemoveDeleted( u8* d64 )
{
	u8* fileRefs[D64_MAX_FILES];
	u8* sectRefs[D64_MAX_FILES];

	u8* curr = d64 + d64Sector( 18, 0 );
	size_t numRefs = 0;
	size_t numReal = 0;
	while( curr[ 0 ] && numRefs < D64_MAX_FILES ) {
		curr = d64 + d64Sector( curr[ 0 ], curr[ 1 ] );
		u8* file = curr;
		while( ( file - curr ) < 256 && numRefs < D64_MAX_FILES ) {
			sectRefs[ numRefs ]  = curr;
			fileRefs[ numRefs++ ] = file;
			if( file[2] ) { numReal++; }
			file += D64_FILE_ENTRY_SIZE;
		}
	}

	size_t currRef = 0;
	curr = d64 + d64Sector( 18, 0 );
	u8* lastSector = curr;
	while( curr[ 0 ] && currRef < numReal ) {
		curr = d64 + d64Sector( curr[ 0 ], curr[ 1 ] );
		u8* file = curr;
		while( ( file - curr ) < 256 ) {
			if( file[ 2 ] ) {
				printf( "%2d: ", (int)currRef );
				memcpy( fileRefs[currRef++] + 2, file + 2, 32-2 );
				printFile( fileRefs[currRef-1] );

				if( currRef == numReal ) {
					file = fileRefs[ currRef-1 ];
					curr = sectRefs[ currRef-1 ];
					lastSector = curr;
					file += D64_FILE_ENTRY_SIZE;
					while( ( file - curr ) < 256 && numRefs < D64_MAX_FILES ) {
						memset( file + 2, 0, D64_FILE_ENTRY_SIZE - 2 );
						file += D64_FILE_ENTRY_SIZE;
					}
					break;
				}

			}
			file += D64_FILE_ENTRY_SIZE;

		}
	}

	// ok, really destroy remaining files...
	u8* bam = d64 + d64Sector( 18, 0 ) + 4;
	u8 nextTrack = lastSector[ 0 ];
	u8 nextSector = lastSector[ 1 ];
	lastSector[ 0 ] = 0;
	lastSector[ 1 ] = 0;
	while( nextTrack && nextSector ) {
		int bamBit = nextTrack * 32 + SectorsPerTrack[ nextTrack ] - nextSector - 1;
		bam[ bamBit >> 3 ] |= 1 << ( bamBit & 7 );
		u8* sector = d64 + d64Sector( nextTrack, nextSector );
		nextTrack = sector[ 0 ];
		nextSector = sector[ 1 ];
		sector[0] = 0;
		sector[1] = 0;
	}


}


u8* d64Merge( u8* data, u8* dir, int skip, int list, int append, int first_index, int chain )
{
	if( list ) { printf( "\nFILES IN DATA:\n" ); }
	int files_in_data = fileCount(data, list, chain);
	if( list ) { printf( "\nFILES IN DIR:\n" ); }
	int files_in_dir = fileCount(dir, list, chain);

	if( chain ) {
		printf("\nCHAIN OF DIRECTORY:\n");
		d64FileChain( data, 18, 1 );
	}

	printf("Number of files in data d64: %d\nNumber of files in dir d64: %d\n", files_in_data, files_in_dir );

	if( ( files_in_data + skip ) > files_in_dir ) {
		printf("Error: files in data d64 plus files to skip exceeds the number of files in the dir art d64.\n");
		return 0;
	}

	// allocate a d64 for the output
	u8* out = (u8*)malloc( D64_SIZE );
	if( out == 0 ) { return out; }

	// all the data on the disk comes from the data d64
	memcpy( out, data, D64_SIZE );

	// the folder structure comes from the dir d64
	// copy disk name and dos type
	int diskNameOffset = d64Sector( 18, 0 ) + 0x90;
	memcpy( out + diskNameOffset, dir + diskNameOffset, 0xab-0x90 );
	// copy track 18 from dir to out
	memcpy( out + d64Sector( 18, 1 ), dir + d64Sector( 18, 1), d64Sector( 19, 0 ) - d64Sector( 18, 1 ) );

	// use the BAM for the directory sector from the directory disk
	int BAMOffs = d64Sector( 18, 0 ) + ( 18 - 1 ) * 4 + 4;
	memcpy( out + BAMOffs, dir + BAMOffs, 4 );

	// copy file type, disk location, file name prefix from the data disk
	int currFile = 0;
	u8* data_curr = data + d64Sector( 18, 0 );
	u8* data_file = 0;
	u8* dir_curr = dir + d64Sector( 18, 0 );
	u8* dir_file = 0;

	int data_files_left = files_in_data;
	(void)first_index;
	if( list ) { printf( "\nFILES IN OUTPUT D64:\n" ); }
	// only update files from the data directory
	int first = first_index+1;
	while( currFile < files_in_dir ) {
		if( data_files_left ) {
			if( !skip || first ) {
				for(;;) { // grab the next data file
					if( !data_file || ( data_file - data_curr ) >= 0x100 ) {
						data_curr = data + d64Sector( data_curr[ 0 ], data_curr[ 1 ] );
						data_file = data_curr;
						break;
					}
					data_file += D64_FILE_ENTRY_SIZE;
					if( (data_file - data_curr ) < 0x100 && data_file[2] ) { break; }
				}
				data_files_left--;
			}
		} else { data_file = 0; }
		for(;;) { // grab the next directory file
			if( !dir_file || ( dir_file - dir_curr ) >= 0x100 ) {
				dir_curr = dir + d64Sector( dir_curr[ 0 ], dir_curr[ 1 ] );
				dir_file = dir_curr;
				break;
			}
			dir_file += D64_FILE_ENTRY_SIZE;
			if( ( dir_file - dir_curr ) < 0x100 && dir_file[ 2 ] ) { break; }
		}
		// the out file is relative to the dir d64
		u8* out_file = out + ( dir_file - dir );

		if( data_file && ( !skip || first==1 ) ) {
			if( append ) {
				int shift = 0;
				for( int i=0; i<16 && data_file[5+i]!=0xa0 && (dir_file[20-i]&0x7f)==0x20; i++ ) { shift++; }
				if( shift && shift<16 ) { memcpy( out_file + 5 + shift, dir_file + 5, 16-shift ); }
			}
			// copy the file info from the data file into the out file
			memcpy( out_file+2, data_file+2, 3 );
			memcpy( out_file + 21, data_file + 21, 11 );
			if( first == 1 ) { memcpy( out_file + 5, data_file + 5, 16 ); }
			// first filename entirely from dir art disk
			if(currFile && !first) {
				for(int i = 5; i<21 && data_file[i] != 0xa0; ++i ) { out_file[i] = data_file[i]; }
			}
		} else {
			out_file[ 2 ] = 0x80; // make sure remaining files are all DEL
			out_file[3] = 0x00;
			out_file[4] = 0x00;
			memset( out_file + 21, 0, 11 );
		}
		if( list ) { printFile( out_file ); }
		++currFile;
		if( skip ) { --skip; }
		if( first ) { --first; }
	}
	return out;
}

void* load(const char* filename, size_t* size_ret)
{
	FILE* f = fopen( filename, "rb" );
	if( f ) {
		fseek( f, 0, SEEK_END );
		size_t size = ftell( f );
		fseek( f, 0, SEEK_SET );
		void* buf = malloc( size );
		if( buf ) {
			fread( buf, size, 1, f );
			if( size_ret ) { *size_ret = size; }
		}
		fclose( f );
		return buf;
	}
	return 0;
}

int save( void* buf, const char* filename, size_t size )
{
	FILE* f = fopen( filename, "wb" );
	if( f ) {
		fwrite( buf, size, 1, f );
		fclose( f );
		return 0;
	}
	return 1;
}

int main( int argc, char* argv[] )
{
	// parse command line arguments
	int skip = 0, list = 0, append = 0, first = 0, chain = 0;
	const char *data = 0, *dir = 0, *out = 0;
	for( int i = 1; i < argc; ++i ) {
		if( argv[ i ][ 0 ] == '-' ) {
			const char* cmd = argv[i]+1;
			const char* eq = strchr( cmd, '=' );
			size_t len = eq ? ( eq - cmd ) : strlen( cmd );
			if( _strnicmp( "skip", cmd, len ) == 0 && eq ) { skip = atoi(eq+1); }
			else if( _strnicmp( "first", cmd, len ) == 0 && eq ) { first = atoi(eq+1); }
			else if( _strnicmp( "list", cmd, len ) == 0 ) { list = 1; }
			else if( _strnicmp( "append", cmd, len ) == 0 ) { append = eq ? atoi(eq+1) : 1; }
			else if( _strnicmp( "chain", cmd, len ) == 0 ) { chain = 1; }
		}
		else if( !data ) { data = argv[i]; }
		else if( !dir ) { dir = argv[i]; }
		else if( !out ) { out = argv[i]; }
		else {
			printf( "Too many file arguments passed in\n" );
			return 1;
		}
	}

	if( !data || !dir || !out )
	{
		printf("usage:\n%s [-skip=x] [-first=x] [-append] [-list] <d64 data> <d64 dir> <d64 out>\n", argv[0] );
		printf("Notes:\n* .d64 files all need different names\n* .d64 dir should have more files than d64 data.\n");
		return 0;
	}

	printf("Files names from \"%s\" will be padded with corresponding file names from\n \"%s\"\n and saved out as a new disk named\n \"%s\".\nPut first file in target index %d.\n",
			data, dir, out, first );

	size_t data_size, dir_size;

	u8* data_d64 = (u8*)load( data, &data_size );
	u8* dir_d64 = ( u8* )load( dir, &dir_size );
	u8* out_d64 = 0;

	int ret = 0;

	if( data_d64 && dir_d64 && data_size == D64_SIZE && dir_size == D64_SIZE ) {
		d64RemoveDeleted( dir_d64 );
		out_d64 = d64Merge( data_d64, dir_d64, skip, list, append, first, chain );
		if( !out_d64 ) { ret = 1; }
		else { ret = save( out_d64, out, D64_SIZE ); }
	} else if( !data_d64 ) {
		printf("Failed to open data disk file \"%s\"\n", data );
		ret = 1;
	} else if( !dir_d64 ) {
		printf("Failed to open directory disk file \"%s\"\n", dir );
		ret = 1;
	} else if( data_size != D64_SIZE ) {
		printf( "Data d64 file \"%s\" is not a standard size (%d bytes)\n", data, D64_SIZE );
		ret = 1;
	} else if( data_size != D64_SIZE ) {
		printf( "Directory d64 file \"%s\" is not a standard size (%d bytes)\n", dir, D64_SIZE );
		ret = 1;
	}

	if( data_d64 ) { free(data_d64); }
	if( dir_d64 ) { free(dir_d64); }
	if( out_d64 ) { free(out_d64); }

	return out_d64 ? 0 : 1;
}