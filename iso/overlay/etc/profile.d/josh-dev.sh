# Josh Developer Mode user-installed agent locations.
for _josh_dev_bin in \
  "$HOME/.local/bin" \
  "$HOME/.local/share/agy/bin" \
  "$HOME/.claude/local/bin"
do
  case ":$PATH:" in
    *":${_josh_dev_bin}:"*) ;;
    *) PATH="${_josh_dev_bin}:$PATH" ;;
  esac
done
export PATH
unset _josh_dev_bin
