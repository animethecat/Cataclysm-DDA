#include "catch/catch.hpp"
#include "mutation.h"

#include <map>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "npc.h"
#include "player.h"
#include "player_helpers.h"
#include "type_id.h"

static std::string get_mutations_as_string( const player &p );

// Note: If a category has two mutually-exclusive mutations (like pretty/ugly for Lupine), the
// one they ultimately end up with depends on the order they were loaded from JSON
static void give_all_mutations( player &p, const mutation_category_trait &category,
                                const bool include_postthresh )
{
    p.set_body();
    const std::vector<trait_id> category_mutations = mutations_category[category.id];

    // Add the threshold mutation first
    if( include_postthresh && !category.threshold_mut.is_empty() ) {
        p.set_mutation( category.threshold_mut );
    }

    for( const auto &m : category_mutations ) {
        const auto &mdata = m.obj();
        if( include_postthresh || ( !mdata.threshold && mdata.threshreq.empty() ) ) {
            int mutation_attempts = 10;
            while( mutation_attempts > 0 && p.mutation_ok( m, false, false ) ) {
                INFO( "Current mutations: " << get_mutations_as_string( p ) );
                INFO( "Mutating towards " << m.c_str() );
                if( !p.mutate_towards( m ) ) {
                    --mutation_attempts;
                }
                CHECK( mutation_attempts > 0 );
            }
        }
    }
}

static int get_total_category_strength( const player &p )
{
    int total = 0;
    for( const auto &i : p.mutation_category_level ) {
        total += i.second;
    }

    return total;
}

// Returns the list of mutations a player has as a string, for debugging
std::string get_mutations_as_string( const player &p )
{
    std::ostringstream s;
    for( auto &m : p.get_mutations() ) {
        s << static_cast<std::string>( m ) << " ";
    }
    return s.str();
}

TEST_CASE( "Having all mutations give correct highest category", "[mutations]" )
{
    for( const auto &cat : mutation_category_trait::get_all() ) {
        const auto &cur_cat = cat.second;
        const auto &cat_id = cur_cat.id;
        if( cat_id == mutation_category_id( "ANY" ) ) {
            continue;
        }
        // Unfinished mutation category.
        if( cur_cat.wip ) {
            continue;
        }

        GIVEN( "The player has all pre-threshold mutations for " + cat_id.str() ) {
            npc dummy;
            give_all_mutations( dummy, cur_cat, false );

            THEN( cat_id.str() + " is the strongest category" ) {
                INFO( "MUTATIONS: " << get_mutations_as_string( dummy ) );
                CHECK( dummy.get_highest_category() == cat_id );
            }
        }

        GIVEN( "The player has all mutations for " + cat_id.str() ) {
            npc dummy;
            give_all_mutations( dummy, cur_cat, true );

            THEN( cat_id.str() + " is the strongest category" ) {
                INFO( "MUTATIONS: " << get_mutations_as_string( dummy ) );
                CHECK( dummy.get_highest_category() == cat_id );
            }
        }
    }
}

TEST_CASE( "Having all pre-threshold mutations gives a sensible threshold breach chance",
           "[mutations]" )
{
    const float BREACH_CHANCE_MIN = 0.2f;
    const float BREACH_CHANCE_MAX = 0.4f;

    for( const auto &cat : mutation_category_trait::get_all() ) {
        const auto &cur_cat = cat.second;
        const auto &cat_id = cur_cat.id;
        if( cat_id == mutation_category_id( "ANY" ) ) {
            continue;
        }
        // Unfinished mutation category.
        if( cur_cat.wip ) {
            continue;
        }

        GIVEN( "The player has all pre-threshold mutations for " + cat_id.str() ) {
            npc dummy;
            give_all_mutations( dummy, cur_cat, false );

            const int category_strength = dummy.mutation_category_level[cat_id];
            const int total_strength = get_total_category_strength( dummy );
            float breach_chance = category_strength / static_cast<float>( total_strength );

            THEN( "Threshold breach chance is at least 0.2" ) {
                INFO( "MUTATIONS: " << get_mutations_as_string( dummy ) );
                CHECK( breach_chance >= BREACH_CHANCE_MIN );
            }
            THEN( "Threshold breach chance is at most 0.4" ) {
                INFO( "MUTATIONS: " << get_mutations_as_string( dummy ) );
                CHECK( breach_chance <= BREACH_CHANCE_MAX );
            }
        }
    }
}

TEST_CASE( "Scout and Topographagnosia traits affect overmap sight range", "[mutations][overmap]" )
{
    Character &dummy = get_player_character();
    clear_avatar();

    WHEN( "character has Scout trait" ) {
        dummy.toggle_trait( trait_id( "EAGLEEYED" ) );
        THEN( "they have increased overmap sight range" ) {
            CHECK( dummy.mutation_value( "overmap_sight" ) == 5 );
        }
        // Regression test for #42853
        THEN( "the Self-Aware trait does not affect overmap sight range" ) {
            dummy.toggle_trait( trait_id( "SELFAWARE" ) );
            CHECK( dummy.mutation_value( "overmap_sight" ) == 5 );
        }
    }

    WHEN( "character has Topographagnosia trait" ) {
        dummy.toggle_trait( trait_id( "UNOBSERVANT" ) );
        THEN( "they have reduced overmap sight range" ) {
            CHECK( dummy.mutation_value( "overmap_sight" ) == -10 );
        }
        // Regression test for #42853
        THEN( "the Self-Aware trait does not affect overmap sight range" ) {
            dummy.toggle_trait( trait_id( "SELFAWARE" ) );
            CHECK( dummy.mutation_value( "overmap_sight" ) == -10 );
        }
    }
}
