#!/usr/bin/env bash
_pinctrl ()
{
    local cur prev words cword split cmd pins pincomp ALLPINS CHIPS chip pinmode=false prefix arg i func pull;
    local opts="no op ip a0 a1 a2 a3 a4 a5 a6 a7 a8 dh dl pu pd pn ";
    _init_completion -s || return;

    i=1
    while [[ $i -lt $cword && ${COMP_WORDS[$i]} =~ ^- ]]; do
        arg=${COMP_WORDS[$i]}
        if [[ "$arg" == "-c" ]]; then
            chip=${COMP_WORDS[$((i + 1))]}
            i=$((i + 2))
        else
            if [[ "$arg" == "-p" ]]; then
                pinmode=true
            fi
            i=$((i + 1))
        fi
    done

    if [[ $i -lt $cword && ${COMP_WORDS[$i]} =~ ^(get|set|funcs|poll|help) ]]; then
        cmd=${COMP_WORDS[$i]}
        i=$((i + 1))
    elif [[ ${COMP_WORDS[$i]} =~ ^[A-Z0-9] ]]; then
        cmd="set"
    fi

    if [[ "$cmd" != "" && $i -lt $cword ]]; then
        pins=${COMP_WORDS[$i]};
        i=$((i + 1))
        while [[ $i -lt $cword && ${COMP_WORDS[$i]} =~ ^[-,] ]]; do
            pins="$pins${COMP_WORDS[$i]}"
            i=$((i + 1))
        done
    fi

    if [[ "$pins" != "" ]]; then
        while [[ $i -lt $cword ]]; do
            arg=${COMP_WORDS[$i]}
            if [[ $arg =~ ^(no|ip|op|a[0-8])$ ]]; then
                func=$arg
                opts=$(echo "$opts" | sed -E "s/(no|ip|op|a[0-8]) //g")
                if [[ $func != "op" ]]; then
                    opts=$(echo "$opts" | sed -E "s/(dh|dl) //g")
                fi
            elif [[ $arg =~ ^(pu|pd|pn)$ ]]; then
                pull=$arg
                opts=$(echo "$opts" | sed -E "s/(pu|pd|pn) //g")
            fi
            i=$((i + 1))
        done
    fi

    if [[ "$cmd" != "" && ( "$pins" == "" || "${cur}" =~ [-,] ) ]]; then
        prefix=$(echo $cur | sed 's/[^-,]*$//')
        if $pinmode; then
            ALLPINS={1..40}
        else
            ALLPINS=($(pinctrl get | cut -d'/' -f4 | cut -d' ' -f1) $(pinctrl get | cut -d'/' -f3 | cut -d' ' -f2))
        fi
        pincomp="${ALLPINS[@]/#/$prefix}"
        COMPREPLY+=($(compgen -W "$pincomp" -- $cur))
    elif [[ "$cmd" != "" ]]; then
        if [[ "$cmd" == "set" ]]; then
            COMPREPLY+=($(compgen -W "$opts" -- $cur))
        fi
    else
        if [[ "$prev" == "-c" ]]; then
            CHIPS=($(pinctrl -v -p 0 | grep 'gpios)' | cut -d' ' -f4 | sort | uniq))
            chips="${CHIPS[@]}"
            COMPREPLY+=($(compgen -W "$chips" -- $cur))
        elif [[ "$cur" =~ ^- ]]; then
            COMPREPLY+=($(compgen -W "-p -h -v -c" -- $cur))
        elif [[ "$chip" == "" ]]; then
            COMPREPLY+=($(compgen -W "get set poll funcs help" -- $cur))
        else
            COMPREPLY+=($(compgen -W "funcs help" -- $cur))
        fi
    fi
}

complete -F _pinctrl pinctrl
