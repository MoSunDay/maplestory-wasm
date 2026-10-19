## WZ to NX (Docker, NoLifeWzToNx)

This workflow runs fully inside Docker:
- clones `https://github.com/ryantpayton/NoLifeWzToNx`
- builds the converter
- converts `wz/*.wz` to `nxFiles/*.nx`

No host-side Go installation is required.

### Run

```bash
./scripts/wz-converter/convert.sh
```

Or directly:

```bash
docker compose -f scripts/wz-converter/docker-compose.yml run --rm wz-converter
```

### Inputs/Outputs

- Input WZ files: `wz/*.wz`
- Cached clone: `.cache/tools/NoLifeWzToNx/`
- Built binary: `.cache/tools/bin/NoLifeWzToNx`
- Converted output: `nxFiles/*.nx`

### Environment knobs

Set with compose overrides or shell env before running:

- `NO_LIFE_WZ_TO_NX_REF` (default: `master`)
- `MODE` (`client` or `server`, default: `client`)
- `LZ4HC` (`1` enables `-h` high-compression mode, default: `0`)
- `SKIP_LIST_WZ` (`1` skips `List.wz`, default: `1`)

## Post-convert: NX 注入条目（勿丢）

`assets/Npc.nx` 与 `assets/String.nx` 含有本管线产物之外的注入条目（NPC
`9270052.img` 妮娜：079MAX4 立绘/铭牌，见 `features/changelog/2026-10-19/`）。
若重跑本管线并把 `nxFiles/*.nx` 拷回 `assets/`，必须随后重新注入并校验：

```bash
python3 tools/nx/inject_npc_nx.py --npc-id 9270052   # 默认读 .ref-079max4 基准
python3 tools/nx/verify_npc_nx.py --npc-id 9270052
```

注入器幂等（重复执行会拒绝并保留最早备份）。注入前的原始文件备份为
`assets/*.pre-9270052-backup`。
