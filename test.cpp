#define THREADS 5
#define MAX_SLEEP 5
#define ITEMS 1000
#define LOOP 100

#include <array>
#include <vector>
#include <thread>
#include <cstdlib>
#include <utility>
#include <unistd.h>
#include <iostream>

#include "lfpool.hpp"

typedef lfs::lfPool< int > Pool;
typedef std::pair< int *, int > Data;
typedef std::vector< std::pair< int *, int > > Vector;

void testFunction( int name, Pool *p, Vector *v ) {
  int items = 0;
  int loop = 0;

  std::cout << "Iniciando hilo " << name << ".\n";

  while( items < ITEMS ) {
    while( loop < LOOP ) {
      int *pos = p->get( );
      int val = rand( );

      v->push_back( std::make_pair( pos, val ) );
    }

    std::cout << "Pausando hilo " << name << ".\n";
    sleep( rand( ) % MAX_SLEEP );
    std::cout << "Continuando hilo " << name << ".\n";
  }

  std::cout << "Hilo " << name << "finalizado.\n";
}

bool checkVector( const Vector &v ) {
  for( auto const &idx: v ) {
    if( *idx.first != idx.second )
      return false;
  }

  return true;
}

int main( ) {
  lfs::lfPool< int > pool( ITEMS * THREADS );
  std::array< std::thread, THREADS > threads;
  std::array< Vector, THREADS > vectors;

  for( int idx = 0; idx < THREADS; ++idx ) {
    threads[idx] = std::thread( testFunction, idx, &pool, &vectors[idx] );
  }

  for( int idx = 0; idx < THREADS; ++idx ) {
    threads[idx].join( );
  }

  {
    bool error = false;
    for( int idx = 0; idx < THREADS; ++idx ) {
      if( !checkVector( vectors[idx] ) ) {
        std::cout << "¡ ERROR ! en los datos del vector " << idx << "\n";
        error = true;
        break;
      }
    }

    if( !error ) {
      std::cout << "Test pasado SIN errores\n" << std::endl;
    }
  }

  std::cout << std::endl;

  return 0;
}
