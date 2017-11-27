#ifndef LFPOOL_HPP
#define LFPOOL_HPP

#include <atomic>
#include <cassert>
#include <cstddef>

namespace lfs {

template< typename T > class lfPool {
  struct Slot {
    std::atomic_flag busy;
    union {
      T data;
      char dummy;
    };
  
    ~Slot( ) { }
    Slot( ) { }
  };

  struct Block {
    Block *next;
    Slot slots[1];
  };

  size_t slotsCount;
  volatile Block *block;

  volatile Block *newBlock( ) {
    Block *blk = reinterpret_cast< Block * >( new char[sizeof( Block ) + ( sizeof( Slot ) * ( slotsCount - 1 ) )] );
    for( size_t idx = 0; idx < slotsCount; ++idx ) {
      blk->slots[idx].busy.clear( );
    }

    return const_cast< volatile Block * >( blk );
  }

public:
  ~lfPool( ) {
    Block *cur = const_cast< Block * >( block );

    while( cur ) {
      for( size_t idx = 0; idx < slotsCount; ++idx ) {
        if( cur->slots[idx].busy.test_and_set( ::std::memory_order_relaxed ) ) {
          cur->slots[idx].data.~T( );
        }
      }
      Block *tmp = cur->next;
      delete cur;
      cur = tmp;
    }
  }
  lfPool( size_t count ) : slotsCount( count ), block( nullptr ) {
    assert( count );

    volatile Block *tmp = newBlock( );
    tmp->next = nullptr;

    block = tmp;
  }

  T *get( bool = true ) {
    Block *blk = const_cast< Block * >( block );

    while( blk ) {
      for( size_t idx = 0; idx < slotsCount; ++idx ) {
        if( !blk->slots[idx].busy.test_and_set( std::memory_order_acquire ) ) {
          return &( blk->slots[idx].data );
        }
      }

      blk = blk->next;
    }

    throw std::bad_alloc( );

    return nullptr;
  }
  void unget( const T *ptr ) {
    Slot *sl = reinterpret_cast< Slot * >( reinterpret_cast< std::atomic_flag * >( ptr ) - 1 );

    sl->busy.clear( std::memory_order_release );
  }
};

}

#endif

