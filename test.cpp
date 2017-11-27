#define THREADS 100
#define ITEMS 1000

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
  std::cout << "Iniciando hilo " << name << ".\n";

  for( int items = 0; items < ITEMS; ++items ) {
    int *pos = p->get( );
    int val = rand( );

    v->operator[]( items ) = std::make_pair( pos, val );

    if( rand( ) % 5 == 1 ) {
      std::cout << "Pausando hilo " << name << ".\n";
      std::this_thread::yield( );
      std::cout << "Continuando hilo " << name << ".\n";
    }
  }

  std::cout << "Hilo " << name << " finalizado.\n";
}

bool checkVector( const Vector &v ) {
  for( auto const &idx: v ) {
    if( *idx.first != idx.second ) {
      return false;
    }
  }

  return true;
}

int main( ) {
  lfs::lfPool< int > pool( ITEMS * THREADS );
  std::array< std::thread, THREADS > threads;
  std::array< Vector, THREADS > vectors;

  for( int idx = 0; idx < THREADS; ++idx ) {
    vectors[idx].reserve( ITEMS );
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
      }
    }

    if( !error ) {
      std::cout << "Test pasado SIN errores\n" << std::endl;
    }
  }

  std::cout << std::endl;

  return 0;
}
