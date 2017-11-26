#ifndef LFPOOL_HPP
#define LFPOOL_HPP

#include <atomic>
#include <cassert>
#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <type_traits>

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

    ~Block( ) { }
    Block( ) { }
  };

  struct EmptyBlock { EmptyBlock *next; };

  volatile std::atomic< Block * > block;
  std::atomic_flag resizing;
#ifndef NDEBUG
  // Genera un error si intentamos hacer algo durante el destructor.
  // true -> Ok.
  // false -> Estamos en el destructor.
  std::atomic_flag destroy;
#endif
  size_t slotsCount;

  Block *newBlock( ) noexcept( false ) {
    assert( destroy.test_and_set( ) );

    size_t alloc = sizeof( Block ) + ( sizeof( Slot ) * ( slotsCount - 1 ) );
    std::cout << "newBlock( ). Allocate " << alloc << " bytes.\n";

    Block *blk = reinterpret_cast< Block * >( new char[alloc] );

    // Inicializamos los slots.
    for( size_t idx = 0; idx < slotsCount; ++idx ) {
      blk->slots[idx].busy.clear( std::memory_order_release );
    }

    return blk;
  }

public:
  static constexpr bool is_dynamic_resizable = true;

  ~lfPool( ) noexcept ( std::is_nothrow_destructible< T >::value ) {
#ifndef NDEBUG
    destroy.clear( );
#endif
    Block *curr = block;

    while( curr ) {
      // Si el destructor no es trivial, lo invocamos.
      if( !std::is_trivially_destructible< T >::value ) {
        for( size_t idx = 0; idx < slotsCount; ++idx ) {
          if( curr->slots[idx].busy.test_and_set( ) ) {
            curr->slots[idx].data.~T( );
          }
        }
      }

      Block *tmp = curr->next;
      delete curr;
      curr = tmp;
    }
  }
  lfPool( ) = delete;
  explicit lfPool( size_t n ) noexcept( false ) :
      block( nullptr ),
      resizing( ATOMIC_FLAG_INIT ),
#ifndef NDEBUG
      destroy( ATOMIC_FLAG_INIT ),
#endif
      slotsCount( n )
  {
    assert( n );

#ifndef NDEBUG
    destroy.test_and_set( ); // NO estamos en el destructor.
#endif
    Block *tmp = newBlock( );
    tmp->next = nullptr;
    block = tmp;
  }
  lfPool( const lfPool & ) = delete;
  lfPool( lfPool &&o ) = delete;
  lfPool &operator=( const lfPool & ) = delete;
  lfPool &operator=( lfPool && ) = delete;

  T *get( bool resize = true ) noexcept( false ) {
    assert( destroy.test_and_set( ) );

    while( true ) {
      Block *curr = block;

      // Recorremos todos los bloques y todos los nodos, buscando 1 libre.
      while( curr ) {
        for( size_t idx = 0; idx < slotsCount; ++idx ) {
          if( !( curr->slots[idx].busy.test_and_set( std::memory_order_acquire ) ) ) {
            // Encontramos un slot libre.
            return &( curr->slots[idx].data );
          }
        }

        curr = curr->next;
      }

      // Si llegamos aquí, es que no hay slots libres en ningún bloque.
      std::cerr << "¡ No hay slots libres !\n";
      abort( );
    }

    return nullptr;
  }
  void unget( T &data ) noexcept {
    assert( destroy.test_and_set( ) );

    Slot *s = reinterpret_cast< Slot * >( reinterpret_cast< std::atomic_flag * >( &data ) - 1 );

    s->~T( );
    s->busy.clear( );
  }

  size_t tsize( ) const { return sizeof( T ); }
  size_t ptrSize( ) const { return sizeof( Block * ); }
  size_t emptyBlockSize( ) const { return sizeof( EmptyBlock ); }
  size_t blockSize( ) const { return sizeof( Block ); }
  size_t slotSize( ) const { return sizeof( Slot ); }
};

}

#endif
