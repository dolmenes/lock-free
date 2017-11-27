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
    volatile Block *next;
    Slot slots[1];
  };

  size_t slotsCount;
  volatile Block *block;

  volatile Block *newBlock( ) noexcept( false ) {
    Block *blk = reinterpret_cast< Block * >( new char[sizeof( Block ) + ( sizeof( Slot ) * ( slotsCount - 1 ) )] );
    for( size_t idx = 0; idx < slotsCount; ++idx ) {
      blk->slots[idx].busy.clear( );
    }

    return const_cast< volatile Block * >( blk );
  }

public:
  ~lfPool( ) noexcept( std::is_nothrow_destructible< T >::value ) {
    Block *cur = const_cast< Block * >( block );

    if( !std::is_trivially_destructible< T >::value ) {
      while( cur ) {
        for( size_t idx = 0; idx < slotsCount; ++idx ) {
          if( cur->slots[idx].busy.test_and_set( ::std::memory_order_relaxed ) ) {
            cur->slots[idx].data.~T( );
          }
        }

        Block *tmp = const_cast< Block * >( cur->next );
        delete cur;
        cur = tmp;
      }
    } 
  }
  explicit lfPool( size_t count ) noexcept( false ) : slotsCount( count ), block( nullptr ) {
    Block *tmp = const_cast< Block * >( newBlock( ) );
    tmp->next = nullptr;

    block = const_cast< volatile Block * >( tmp );
  }

  T *get( bool = true ) noexcept( false ) {
    Block *blk = const_cast< Block * >( block );

    while( blk ) {
      for( size_t idx = 0; idx < slotsCount; ++idx ) {
        if( !blk->slots[idx].busy.test_and_set( std::memory_order_acquire ) ) {
          return &( blk->slots[idx].data );
        }
      }

      blk = const_cast< Block * >( blk->next );
    }

    throw std::bad_alloc( );

    return nullptr;
  }
  template< typename... ARGS > T *emplace( ARGS... args ) noexcept( false ) {
    T *tmp = get( );
  
    return new (tmp) T( args... );
  }
  void unget( const T *ptr ) noexcept( std::is_nothrow_destructible< T >::value ) {
    Slot *sl = reinterpret_cast< Slot * >( reinterpret_cast< std::atomic_flag * >( ptr ) - 1 );

    if( !std::is_trivially_destructible< T >::value ) {
      sl->data.~T( );
    }
 
    sl->busy.clear( std::memory_order_release );
  }
};

}

#endif
