#ifndef LFPOOL_HPP
#define LFPOOL_HPP

#include <atomic>
#include <cassert>
#include <cstddef>

namespace lfs {

template< typename T > class lfPool {
  struct Node {
    std::atomic_flag busy = ATOMIC_FLAG_INIT;
    union {
      T data;
      char dummy[sizeof( T )];
    };

    ~Node( ) { }
    Node( ) = default;
    Node( Node && ) = default;
  };

  struct Block {
    Block *next = nullptr;
    Node nodes[1];

    ~Block( ) { }
    Block( ) = default;
    Block( Block && ) = default;
  };

  std::atomic< Block * > block = nullptr;
  std::atomic_flag resizing = ATOMIC_FLAG_INIT;
#ifndef NDEBUG
  // Genera un error si intentamos hacer algo durante el destructor.
  std::atomic_flag destroy = ATOMIC_FLAG_INIT;
#endif
  size_t npb;

  Block *newBlock( ) noexcept( false ) {
    assert( destroy.test_and_set( ) );

    Block *blk = reinterpret_cast< Block * >( new char[sizeof( Block ) + ( sizeof( Node ) * ( npb - 1 ) )] );
    for( size_t idx = 0; idx < npb; ++idx ) {
      new ( &(blk->nodes[idx]) ) Node( );
    }
  
    return blk;
  }
public:
  static constexpr bool is_dynamic_resizable = true;

  ~lfPool( ) noexcept {
#ifndef NDEBUG
    destroy.clear( );
#endif
    Block *curr = block;
    while( block ) {
      Block *tmp = curr->next;
      delete[] block;
      block = tmp;
    }
  }
  lfPool( ) = delete;
  explicit lfPool< T >( size_t n ) noexcept( false ) : npb( n ) {
    block = newBlock( );
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
        for( size_t idx = 0; idx < npb; ++idx ) {
          if( !( curr->nodes[idx].busy.test_and_set( ) ) ) {
            return &curr->nodes[idx].data;
          }
        }

        curr = curr->next;
      }

      if( !resize ) break;

      // Creamos un nuevo Block.
      if( !resizing.test_and_set( ) ) {
        // Tenemos el cerrojo.
        Block *blk = newBlock( );
        blk->next = nullptr;
        // Colocamos el bloque al principio de la lista.
        block.store( blk );
        // Quitamos el cerrojo.
        resizing.clear( );
      }
    }

    return nullptr;
  }
  void unget( T &data ) noexcept {
    assert( destroy.test_and_set( ) );

    Node *n = reinterpret_cast< Node * >( reinterpret_cast< std::atomic_flag * >( &data ) - 1 );

    n.busy.clear( );
  }
};

}

#endif
