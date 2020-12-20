start_server {tags {"tracking"}} {
    # Create a deferred client we'll use to redirect invalidation
    # messages to.
    set rd_redirection [redis_deferring_client]
    $rd_redirection client id
    set redir_id [$rd_redirection read]
    $rd_redirection subscribe __redis__:invalidate
    $rd_redirection read ; # Consume the SUBSCRIBE reply.

    # Create another client that's not used as a redirection client
    # We should always keep this client's buffer clean
    set rd [redis_deferring_client]

    # Client to be used for SET and GET commands
    # We don't read this client's buffer
    set rd_sg [redis_client] 

    proc clean_all {} {
        uplevel {
            $rd QUIT
            $rd_redirection QUIT
            set rd [redis_deferring_client]
            set rd_redirection [redis_deferring_client]
            $rd_redirection client id
            set redir_id [$rd_redirection read]
            $rd_redirection subscribe __redis__:invalidate
            $rd_redirection read ; # Consume the SUBSCRIBE reply.
            r FLUSHALL
            r CLIENT TRACKING off
            r HELLO 2
            r config set tracking-table-max-keys 1000000
        }
    }

    test {Clients are able to enable tracking and redirect it} {
        r CLIENT TRACKING on REDIRECT $redir_id
    } {*OK}

    test {The other connection is able to get invalidations} {
        r SET a 1
        r SET b 1
        r GET a
        r INCR b ; # This key should not be notified, since it wasn't fetched.
        r INCR a
        set keys [lindex [$rd_redirection read] 2]
        assert {[llength $keys] == 1}
        assert {[lindex $keys 0] eq {a}}
    }

    test {The client is now able to disable tracking} {
        # Make sure to add a few more keys in the tracking list
        # so that we can check for leaks, as a side effect.
        r MGET a b c d e f g
        r CLIENT TRACKING off
    }

    test {Clients can enable the BCAST mode with the empty prefix} {
        r CLIENT TRACKING on BCAST REDIRECT $redir_id
    } {*OK*}

    test {The connection gets invalidation messages about all the keys} {
        r MSET a 1 b 2 c 3
        set keys [lsort [lindex [$rd_redirection read] 2]]
        assert {$keys eq {a b c}}
    }

    test {Clients can enable the BCAST mode with prefixes} {
        r CLIENT TRACKING off
        r CLIENT TRACKING on BCAST REDIRECT $redir_id PREFIX a: PREFIX b:
        r MULTI
        r INCR a:1
        r INCR a:2
        r INCR b:1
        r INCR b:2
        r EXEC
        # Because of the internals, we know we are going to receive
        # two separated notifications for the two different prefixes.
        set keys1 [lsort [lindex [$rd_redirection read] 2]]
        set keys2 [lsort [lindex [$rd_redirection read] 2]]
        set keys [lsort [list {*}$keys1 {*}$keys2]]
        assert {$keys eq {a:1 a:2 b:1 b:2}}
    }

    test {Adding prefixes to BCAST mode works} {
        r CLIENT TRACKING on BCAST REDIRECT $redir_id PREFIX c:
        r INCR c:1234
        set keys [lsort [lindex [$rd_redirection read] 2]]
        assert {$keys eq {c:1234}}
    }

    test {Tracking NOLOOP mode in standard mode works} {
        r CLIENT TRACKING off
        r CLIENT TRACKING on REDIRECT $redir_id NOLOOP
        r MGET otherkey1 loopkey otherkey2
        $rd_sg SET otherkey1 1; # We should get this
        r SET loopkey 1 ; # We should not get this
        $rd_sg SET otherkey2 1; # We should get this
        # Because of the internals, we know we are going to receive
        # two separated notifications for the two different prefixes.
        set keys1 [lsort [lindex [$rd_redirection read] 2]]
        set keys2 [lsort [lindex [$rd_redirection read] 2]]
        set keys [lsort [list {*}$keys1 {*}$keys2]]
        assert {$keys eq {otherkey1 otherkey2}}
    }

    test {Tracking NOLOOP mode in BCAST mode works} {
        r CLIENT TRACKING off
        r CLIENT TRACKING on BCAST REDIRECT $redir_id NOLOOP
        $rd_sg SET otherkey1 1; # We should get this
        r SET loopkey 1 ; # We should not get this
        $rd_sg SET otherkey2 1; # We should get this
        # Because of the internals, we know we are going to receive
        # two separated notifications for the two different prefixes.
        set keys1 [lsort [lindex [$rd_redirection read] 2]]
        set keys2 [lsort [lindex [$rd_redirection read] 2]]
        set keys [lsort [list {*}$keys1 {*}$keys2]]
        assert {$keys eq {otherkey1 otherkey2}}
    }

    test {Tracking gets notification of expired keys} {
        r CLIENT TRACKING off
        r CLIENT TRACKING on BCAST REDIRECT $redir_id NOLOOP
        r SET mykey myval px 1
        r SET mykeyotherkey myval ; # We should not get it
        after 1000
        # Because of the internals, we know we are going to receive
        # two separated notifications for the two different prefixes.
        set keys1 [lsort [lindex [$rd_redirection read] 2]]
        set keys [lsort [list {*}$keys1]]
        assert {$keys eq {mykey}}
    }

    test {HELLO 3 reply is correct} {
        set reply [r HELLO 3]
        assert {[lindex $reply 2] eq {proto 3}}
    }

    test {RESP3 based basic invalidation} {
        r CLIENT TRACKING off
        r CLIENT TRACKING on
        $rd_sg SET key1 1
        r GET key1
        $rd_sg SET key1 2
        r read
    } {invalidate key1}

    test {RESP3 tracking redirection} {
        r CLIENT TRACKING off
        r CLIENT TRACKING on REDIRECT $redir_id
        $rd_sg SET key1 1
        r GET key1
        $rd_sg SET key1 2
        set res [lindex [$rd_redirection read] 2]
        assert {$res eq {key1}}
    }

    test {Invalidations of previous keys can be redirected after switching to RESP3} {
        $rd_sg SET key1 1
        r GET key1
        r HELLO 3
        $rd_sg SET key1 2
        set res [lindex [$rd_redirection read] 2]
        assert {$res eq {key1}}
    }

    test {Invalidations of new keys can be redirected after switching to RESP3} {
        r HELLO 3
        $rd_sg SET key1 1
        r GET key1
        $rd_sg SET key1 2
        set res [lindex [$rd_redirection read] 2]
        assert {$res eq {key1}}
    }

    test {RESP3 Client gets tracking-redir-broken push message after cached key changed when rediretion client is terminated} {
        r CLIENT TRACKING on REDIRECT $redir_id
        $rd_sg SET key1 1
        r GET key1
        $rd_redirection QUIT
        $rd_sg SET key1 2
        set MAX_TRIES 100
        for {set i 0} {$i <= $MAX_TRIES && $res <= 0} {incr i} {
            set res [lsearch -exact [r PING] "tracking-redir-broken"]
        }
        assert {$res >= 0}
    }

    test {Different clients can redirect to the same connection} {
        # Reinstantiating after QUIT
        set rd_redirection [redis_deferring_client]
        $rd_redirection CLIENT ID
        set redir_id [$rd_redirection read]
        $rd_redirection SUBSCRIBE __redis__:invalidate
        $rd_redirection read ; # Consume the SUBSCRIBE reply
        r CLIENT TRACKING on REDIRECT $redir_id
        $rd CLIENT TRACKING on REDIRECT $redir_id 
        $rd read ; # Consume the TRACKING reply
        $rd_sg MSET key1 1 key2 1
        r GET key1
        $rd GET key2 
        $rd read ; # Consume the GET reply
        $rd_sg INCR key1
        $rd_sg INCR key2
        set res1 [lindex [$rd_redirection read] 2]
        set res2 [lindex [$rd_redirection read] 2]
        assert {$res1 eq {key1}}
        assert {$res2 eq {key2}}
    }

    test {Different clients using different protocols can track the same key} {
        $rd HELLO 3 
        $rd read ; # Consume the HELLO reply
        $rd CLIENT TRACKING on 
        $rd read ; # Consume the TRACKING reply
        $rd_sg set key1 1
        r GET key1
        $rd GET key1 
        $rd read ; # Consume the GET reply
        $rd_sg INCR key1
        set res1 [lindex [$rd_redirection read] 2]
        $rd PING ; # Non redirecting client has to talk to the server in order to get invalidation message
        set res2 [lindex [split [$rd read] " "] 1] 
        $rd read ; # Consume the PING reply, which comes together with the invalidation message
        assert {$res1 eq {key1}}
        assert {$res2 eq {key1}}
    }

    test {No invalidation message when using OPTIN option} {
        r CLIENT TRACKING on OPTIN REDIRECT $redir_id
        $rd_sg SET key1 1
        r GET key1 ; # This key should not be notified, since OPTIN is on and CLIENT CACHING yes wasn't called
        $rd_sg SET key1 2
        # Preparing some message to consume on $rd_redirection so we don't get blocked
        r CLIENT TRACKING off
        r CLIENT TRACKING on REDIRECT $redir_id
        $rd_sg SET key2 1
        r GET key2 ; # This key should be notified
        $rd_sg SET key2 2
        set res [lindex [$rd_redirection read] 2]
        assert {$res eq {key2}}
    }

    test {Invalidation message sent when using OPTIN option with CLIENT CACHING yes} {
        r CLIENT TRACKING on OPTIN REDIRECT $redir_id
        $rd_sg SET key1 3
        r CLIENT CACHING yes
        r GET key1
        $rd_sg SET key1 4
        set res [lindex [$rd_redirection read] 2]
        assert {$res eq {key1}}
    }

    test {Invalidation message sent when using OPTOUT option} {
        r CLIENT TRACKING off
        r CLIENT TRACKING on OPTOUT REDIRECT $redir_id
        $rd_sg SET key1 1
        r GET key1 
        $rd_sg SET key1 2
        set res [lindex [$rd_redirection read] 2]
        assert {$res eq {key1}}
    }

    test {No invalidation message when using OPTOUT option with CLIENT CACHING no} {
        $rd_sg SET key1 1
        r CLIENT CACHING no
        r GET key1 ; # This key should not be notified, since OPTOUT is on and CLIENT CACHING no was called
        $rd_sg SET key1 2
        # Preparing some message to consume on $rd_redirection so we don't get blocked
        $rd_sg SET key2 1
        r GET key2 ; # This key should be notified
        $rd_sg SET key2 2
        set res [lindex [$rd_redirection read] 2]
        assert {$res eq {key2}}
    }

    test {Able to redirect to a RESP3 client} {
        $rd_redirection UNSUBSCRIBE __redis__:invalidate ; # Need to unsub first before we can do HELLO 3
        $rd_redirection read ; # Consume the UNSUBSCRIBE reply
        $rd_redirection HELLO 3
        $rd_redirection read ; # Consume the HELLO reply
        $rd_redirection SUBSCRIBE __redis__:invalidate
        $rd_redirection read ; # Consume the SUBSCRIBE reply
        r CLIENT TRACKING on REDIRECT $redir_id
        $rd_sg SET key1 1
        r GET key1
        $rd_sg INCR key1
        set res [lindex [$rd_redirection read] 1]
        assert {$res eq {key1}}
    }

    test {After switching from normal tracking to BCAST mode, no invalidation message is produced for pre-BCAST keys} {
        $rd HELLO 3
        $rd read ; # Consume the HELLO reply
        $rd CLIENT TRACKING on
        $rd read ; # Consume the TRACKING reply
        $rd_sg SET key1 1
        $rd GET key1
        $rd read ; # Consume the GET reply
        $rd CLIENT TRACKING off 
        $rd read ; # Consume the TRACKING reply
        $rd CLIENT TRACKING on BCAST
        $rd read ; # Consume the TRACKING reply
        $rd_sg INCR key1
        $rd PING
        set inv_msg [$rd read]
        set ping_reply [$rd read]
        assert {$inv_msg eq {invalidate key1}}
        assert {$ping_reply eq {PONG}}
    } 

    test {Tracking gets notification on tracking table key eviction} {
        $rd_redirection HELLO 2
        r CLIENT TRACKING off
        r CLIENT TRACKING on REDIRECT $redir_id NOLOOP
        r MSET key1 1 key2 2
        # Let the server track the two keys for us
        r MGET key1 key2
        # Force the eviction of all the keys but one:
        r config set tracking-table-max-keys 1
        # Note that we may have other keys in the table for this client,
        # since we disabled/enabled tracking multiple time with the same
        # ID, and tracking does not do ID cleanups for performance reasons.
        # So we check that eventually we'll receive one or the other key,
        # otherwise the test will die for timeout.
        while 1 {
            set keys [lindex [$rd_redirection read] 2]
            if {$keys eq {key1} || $keys eq {key2}} break
        }
        # We should receive an expire notification for one of
        # the two keys (only one must remain)
        assert {$keys eq {key1} || $keys eq {key2}}
    }

    test {Invalidation message received for flushall} {
        clean_all
        r CLIENT TRACKING off
        r CLIENT TRACKING on REDIRECT $redir_id
        $rd_sg SET key1 1
        r GET key1
        $rd_sg FLUSHALL
        set msg [$rd_redirection read]
        assert {[lindex msg 2] eq {} }
    }

    test {Invalidation message received for flushdb} {
        clean_all
        r CLIENT TRACKING off
        r CLIENT TRACKING on REDIRECT $redir_id
        $rd_sg SET key1 1
        r GET key1
        $rd_sg FLUSHDB
        set msg [$rd_redirection read]
        assert {[lindex msg 2] eq {} }
    }

    # Keys are defined to be evicted 100 at a time by default.
    # If after eviction the number of keys still surpasses the limit
    # defined in tracking-table-max-keys, we increases eviction 
    # effort to 200, and then 300, etc. 
    # This test tests this effort incrementation. 
    test {Server is able to evacuate enough keys when num of keys surpasses limit by more than defined initial effort} {
        clean_all
        set NUM_OF_KEYS_TO_TEST 250
        set TRACKING_TABLE_MAX_KEYS 1
        r CLIENT TRACKING on REDIRECT $redir_id
        for {set i 0} {$i < $NUM_OF_KEYS_TO_TEST} {incr i} {
            $rd_sg SET key$i $i
            r GET key$i
        }
        r config set tracking-table-max-keys $TRACKING_TABLE_MAX_KEYS
        # If not enough keys are evicted, we won't get enough invalidation
        # messages, and "$rd_redirection read" will block.
        # If too many keys are evicted, we will get too many invalidation
        # messages, and the assert will fail.
        for {set i 0} {$i < $NUM_OF_KEYS_TO_TEST - $TRACKING_TABLE_MAX_KEYS} {incr i} {
            $rd_redirection read
        }
        $rd_redirection PING
        assert {[$rd_redirection read] eq {pong {}}}
    }

    test {Tracking info is correct} {
        clean_all
        r CLIENT TRACKING on REDIRECT $redir_id
        $rd_sg SET key1 1
        $rd_sg SET key2 2
        r GET key1 
        r GET key2
        $rd CLIENT TRACKING on BCAST PREFIX prefix:
        $rd_sg SET prefix:key1 1 
        $rd_sg SET prefix:key2 2
        set info [r info]
        regexp "\r\ntracking_total_items:(.*?)\r\n" $info _ total_items
        regexp "\r\ntracking_total_keys:(.*?)\r\n" $info _ total_keys
        regexp "\r\ntracking_total_prefixes:(.*?)\r\n" $info _ total_prefixes
        assert {$total_items == 2}
        assert {$total_keys == 2}
        assert {$total_prefixes == 1}
    }

    $rd_redirection close
    $rd close
}
